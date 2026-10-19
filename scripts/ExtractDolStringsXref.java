// Ghidra Headless Script (@static-analyst) para Inazuma Eleven GO Strikers 2013 (Wii)
// Mapeia as secoes do main.dol (PowerPC 750CL / Broadway 32-bit Big-Endian),
// executa analise estatica basica (funcoes, registradores EABI r3-r10, SDA r2/r13)
// e extrai todas as strings legiveis (ASCII/UTF-8/Shift-JIS), tabelas de ponteiros
// e referencias cruzadas (XREFs) de codigo e dados para output/symbols/strings_xref.json.
//@category Strikers2013.StaticAnalysis
//@author @static-analyst

import ghidra.app.script.GhidraScript;
import ghidra.program.database.mem.FileBytes;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.ProgramContext;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

public class ExtractDolStringsXref extends GhidraScript {

    // Constantes do cabecalho DOL (0x00..0xFF, Big-Endian)
    private static final int DOL_HEADER_SIZE = 0x100;
    private static final int NUM_TEXT_SECTIONS = 7;
    private static final int NUM_DATA_SECTIONS = 11;

    // Registradores base descobertos em __init_registers (0x80004284..0x80004298)
    // 0x80004284: lis r1, 0x809D; ori r1, r1, 0x5DB0 -> SP = 0x809D5DB0
    // 0x8000428C: lis r2, 0x8051; ori r2, r2, 0x67E0 -> _SDA2_BASE_ = 0x805167E0 (.sdata2 + 0x8000)
    // 0x80004294: lis r13, 0x8051; ori r13, r13, 0x4400 -> _SDA_BASE_ = 0x80514400 (.sdata + 0x8000)
    private static final long DEFAULT_STACK_ADDR = 0x809D5DB0L;
    private static final long DEFAULT_SDA2_BASE  = 0x805167E0L;
    private static final long DEFAULT_SDA_BASE   = 0x80514400L;

    private static class DolSection {
        final String name;
        final String role;
        final int index;
        final boolean isText;
        final long fileOffset;
        final long vaddr;
        final long size;
        final byte[] data;

        DolSection(String name, String role, int index, boolean isText, long fileOffset, long vaddr, long size, byte[] data) {
            this.name = name;
            this.role = role;
            this.index = index;
            this.isText = isText;
            this.fileOffset = fileOffset;
            this.vaddr = vaddr;
            this.size = size;
            this.data = data;
        }

        long endAddr() {
            return vaddr + size;
        }
    }

    private static class CodeXref {
        final long fromAddr;
        final long functionAddr;
        final String functionName;
        final long targetAddr;
        final String refType;
        final String mnemonic;
        final String registerName;
        final String eabiRole;
        final Long viaPointerAddr;

        CodeXref(long fromAddr, long functionAddr, String functionName, long targetAddr,
                 String refType, String mnemonic, String registerName, String eabiRole, Long viaPointerAddr) {
            this.fromAddr = fromAddr;
            this.functionAddr = functionAddr;
            this.functionName = functionName;
            this.targetAddr = targetAddr;
            this.refType = refType;
            this.mnemonic = mnemonic;
            this.registerName = registerName;
            this.eabiRole = eabiRole;
            this.viaPointerAddr = viaPointerAddr;
        }
    }

    private static class DataXref {
        final long fromAddr;
        final String fromSection;
        final long targetAddr;
        final Long tableAddr;
        final Integer tableIndex;

        DataXref(long fromAddr, String fromSection, long targetAddr, Long tableAddr, Integer tableIndex) {
            this.fromAddr = fromAddr;
            this.fromSection = fromSection;
            this.targetAddr = targetAddr;
            this.tableAddr = tableAddr;
            this.tableIndex = tableIndex;
        }
    }

    private static class ExtractedString {
        final long address;
        final long fileOffset;
        final String sectionName;
        final String encoding;
        final int byteLength;
        final String value;
        final List<CodeXref> codeXrefs = new ArrayList<>();
        final List<DataXref> dataXrefs = new ArrayList<>();

        ExtractedString(long address, long fileOffset, String sectionName, String encoding, int byteLength, String value) {
            this.address = address;
            this.fileOffset = fileOffset;
            this.sectionName = sectionName;
            this.encoding = encoding;
            this.byteLength = byteLength;
            this.value = value;
        }
    }

    private static class PointerTableEntry {
        final int index;
        final long entryAddr;
        final long targetAddr;
        final String targetSection;
        final String targetKind;
        final String targetPreview;

        PointerTableEntry(int index, long entryAddr, long targetAddr, String targetSection, String targetKind, String targetPreview) {
            this.index = index;
            this.entryAddr = entryAddr;
            this.targetAddr = targetAddr;
            this.targetSection = targetSection;
            this.targetKind = targetKind;
            this.targetPreview = targetPreview;
        }
    }

    private static class PointerTable {
        final long address;
        final long fileOffset;
        final String sectionName;
        final String category;
        final int entryCount;
        final int byteSize;
        final List<PointerTableEntry> entries = new ArrayList<>();
        final List<CodeXref> codeXrefs = new ArrayList<>();
        final List<DataXref> dataXrefs = new ArrayList<>();

        PointerTable(long address, long fileOffset, String sectionName, String category, int entryCount, int byteSize) {
            this.address = address;
            this.fileOffset = fileOffset;
            this.sectionName = sectionName;
            this.category = category;
            this.entryCount = entryCount;
            this.byteSize = byteSize;
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String outputPath = (args != null && args.length > 0 && !args[0].isEmpty())
                ? args[0]
                : "output/symbols/strings_xref.json";

        println("[ ExtractDolStringsXref ] Iniciando analise estatica headless de main.dol...");

        Memory memory = currentProgram.getMemory();
        List<FileBytes> allFileBytes = memory.getAllFileBytes();
        if (allFileBytes.isEmpty()) {
            throw new IllegalStateException("FileBytes nao encontrado no programa Ghidra importado.");
        }
        FileBytes fileBytes = allFileBytes.get(0);
        long totalFileSize = fileBytes.getSize();
        byte[] rawDol = new byte[(int) totalFileSize];
        fileBytes.getModifiedBytes(0, rawDol, 0, (int) totalFileSize);

        ByteBuffer hdrBuf = ByteBuffer.wrap(rawDol, 0, DOL_HEADER_SIZE).order(ByteOrder.BIG_ENDIAN);
        long[] textOff = new long[NUM_TEXT_SECTIONS];
        long[] dataOff = new long[NUM_DATA_SECTIONS];
        long[] textAddr = new long[NUM_TEXT_SECTIONS];
        long[] dataAddr = new long[NUM_DATA_SECTIONS];
        long[] textSize = new long[NUM_TEXT_SECTIONS];
        long[] dataSize = new long[NUM_DATA_SECTIONS];

        for (int i = 0; i < NUM_TEXT_SECTIONS; i++) textOff[i] = Integer.toUnsignedLong(hdrBuf.getInt(0x00 + i * 4));
        for (int i = 0; i < NUM_DATA_SECTIONS; i++) dataOff[i] = Integer.toUnsignedLong(hdrBuf.getInt(0x1C + i * 4));
        for (int i = 0; i < NUM_TEXT_SECTIONS; i++) textAddr[i] = Integer.toUnsignedLong(hdrBuf.getInt(0x48 + i * 4));
        for (int i = 0; i < NUM_DATA_SECTIONS; i++) dataAddr[i] = Integer.toUnsignedLong(hdrBuf.getInt(0x64 + i * 4));
        for (int i = 0; i < NUM_TEXT_SECTIONS; i++) textSize[i] = Integer.toUnsignedLong(hdrBuf.getInt(0x90 + i * 4));
        for (int i = 0; i < NUM_DATA_SECTIONS; i++) dataSize[i] = Integer.toUnsignedLong(hdrBuf.getInt(0xAC + i * 4));

        long bssAddr = Integer.toUnsignedLong(hdrBuf.getInt(0xD8));
        long bssSize = Integer.toUnsignedLong(hdrBuf.getInt(0xDC));
        long entryPoint = Integer.toUnsignedLong(hdrBuf.getInt(0xE0));

        String[] textRoles = {".init", ".text", ".text2", ".text3", ".text4", ".text5", ".text6"};
        String[] dataRoles = {"extab", "extabindex", ".ctors", ".dtors", ".rodata", ".data", ".sdata", ".sdata2", ".data8", ".data9", ".data10"};

        List<DolSection> sections = new ArrayList<>();
        for (int i = 0; i < NUM_TEXT_SECTIONS; i++) {
            if (textSize[i] > 0) {
                byte[] secBytes = new byte[(int) textSize[i]];
                System.arraycopy(rawDol, (int) textOff[i], secBytes, 0, (int) textSize[i]);
                sections.add(new DolSection(".text" + i, textRoles[i], i, true, textOff[i], textAddr[i], textSize[i], secBytes));
            }
        }
        for (int i = 0; i < NUM_DATA_SECTIONS; i++) {
            if (dataSize[i] > 0) {
                byte[] secBytes = new byte[(int) dataSize[i]];
                System.arraycopy(rawDol, (int) dataOff[i], secBytes, 0, (int) dataSize[i]);
                sections.add(new DolSection(".data" + i, dataRoles[i], i, false, dataOff[i], dataAddr[i], dataSize[i], secBytes));
            }
        }

        // 1. Remapeia os blocos de memoria no projeto Ghidra para refletir os enderecos virtuais reais do Wii (0x80xxxxxx)
        remapDolMemoryBlocks(memory, fileBytes, sections, bssAddr, bssSize, entryPoint);

        // 2. Identifica limites de memoria valida
        long minVaddr = Long.MAX_VALUE;
        long maxVaddr = bssAddr + bssSize;
        for (DolSection sec : sections) {
            if (sec.vaddr < minVaddr) minVaddr = sec.vaddr;
            if (sec.endAddr() > maxVaddr) maxVaddr = sec.endAddr();
        }

        // 3. Constroi mapa de palavras de 32 bits Big-Endian nas secoes de dados e identifica todos os ponteiros validos
        Map<Long, Long> dataPointerMap = new HashMap<>(32768);
        Set<Long> pointerTargets = new HashSet<>(16384);
        for (DolSection sec : sections) {
            if (sec.isText) continue;
            ByteBuffer bb = ByteBuffer.wrap(sec.data).order(ByteOrder.BIG_ENDIAN);
            int wordCount = (int) (sec.size / 4);
            for (int i = 0; i < wordCount; i++) {
                long slotAddr = sec.vaddr + (long) i * 4L;
                long val = Integer.toUnsignedLong(bb.getInt(i * 4));
                if (isValidDolTargetAddress(val, sections, bssAddr, bssSize)) {
                    dataPointerMap.put(slotAddr, val);
                    pointerTargets.add(val);
                }
            }
        }

        // 4. Recupera inicios de funcoes em .text0 e .text1 (alvos de BL, prologos stwu r1, ponteiros em .ctors/vtables)
        TreeSet<Long> functionStarts = recoverFunctions(sections, entryPoint, pointerTargets);
        println(String.format("[ ExtractDolStringsXref ] Funcoes recuperadas em .text0/.text1: %d", functionStarts.size()));

        // 5. Extrai strings legiveis (ASCII/UTF-8 e Shift-JIS)
        Charset sjisCharset = Charset.forName("Shift_JIS");
        TreeMap<Long, ExtractedString> stringMap = extractReadableStrings(sections, pointerTargets, sjisCharset);
        println(String.format("[ ExtractDolStringsXref ] Strings legiveis extraidas: %d", stringMap.size()));

        // 6. Detecta tabelas de ponteiros contiguas nas secoes de dados (.data0..data7)
        TreeMap<Long, PointerTable> pointerTableMap = extractPointerTables(sections, dataPointerMap, stringMap, functionStarts, bssAddr, bssSize);
        println(String.format("[ ExtractDolStringsXref ] Tabelas de ponteiros detectadas: %d", pointerTableMap.size()));

        // Indexa quais slots de 4 bytes pertencem a qual tabela de ponteiros
        Map<Long, PointerTable> slotToTableMap = new HashMap<>(32768);
        Map<Long, Integer> slotToTableIdxMap = new HashMap<>(32768);
        for (PointerTable pt : pointerTableMap.values()) {
            for (PointerTableEntry entry : pt.entries) {
                slotToTableMap.put(entry.entryAddr, pt);
                slotToTableIdxMap.put(entry.entryAddr, entry.index);
            }
        }

        // 7. Registra XREFs de dados (ponteiros em .data0..data7 apontando para strings ou tabelas de ponteiros)
        int totalDataXrefsToStrings = 0;
        int totalDataXrefsToTables = 0;
        for (DolSection sec : sections) {
            if (sec.isText) continue;
            int wordCount = (int) (sec.size / 4);
            for (int i = 0; i < wordCount; i++) {
                long fromAddr = sec.vaddr + (long) i * 4L;
                Long targetAddr = dataPointerMap.get(fromAddr);
                if (targetAddr == null) continue;

                PointerTable enclosingTable = slotToTableMap.get(fromAddr);
                Long tableAddr = (enclosingTable != null) ? enclosingTable.address : null;
                Integer tableIdx = slotToTableIdxMap.get(fromAddr);

                ExtractedString targetStr = stringMap.get(targetAddr);
                if (targetStr != null) {
                    targetStr.dataXrefs.add(new DataXref(fromAddr, sec.name, targetAddr, tableAddr, tableIdx));
                    totalDataXrefsToStrings++;
                }

                PointerTable targetTable = pointerTableMap.get(targetAddr);
                if (targetTable != null && (enclosingTable == null || enclosingTable.address != targetTable.address)) {
                    targetTable.dataXrefs.add(new DataXref(fromAddr, sec.name, targetAddr, tableAddr, tableIdx));
                    totalDataXrefsToTables++;
                }
            }
        }

        // 8. Analisa instrucoes PowerPC 750CL em .text0 e .text1 para extrair XREFs de codigo (diretas, base-relativas e SDA r2/r13)
        int[] codeXrefCounts = analyzePowerPcCodeXrefs(
                sections,
                functionStarts,
                dataPointerMap,
                stringMap,
                pointerTableMap,
                slotToTableMap,
                minVaddr,
                maxVaddr
        );
        int totalCodeXrefsToStrings = codeXrefCounts[0];
        int totalCodeXrefsToTables = codeXrefCounts[1];
        int totalAllCodeRefs = codeXrefCounts[2];

        // 9. Registra referencias e rotulos essenciais no banco do Ghidra para consulta futura
        populateGhidraSymbolsAndReferences(stringMap, pointerTableMap);

        // 10. Exporta o relatorio estruturado completo para output/symbols/strings_xref.json
        File outFile = new File(outputPath);
        if (!outFile.isAbsolute()) {
            outFile = new File( System.getProperty("user.dir"), outputPath );
        }
        File parentDir = outFile.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            parentDir.mkdirs();
        }

        writeJsonReport(
                outFile,
                totalFileSize,
                entryPoint,
                bssAddr,
                bssSize,
                sections,
                functionStarts.size(),
                totalAllCodeRefs,
                totalCodeXrefsToStrings,
                totalDataXrefsToStrings,
                totalCodeXrefsToTables,
                totalDataXrefsToTables,
                stringMap,
                pointerTableMap
        );

        println(String.format("[ ExtractDolStringsXref ] Exportacao concluida com sucesso em: %s", outFile.getAbsolutePath()));
    }

    private void remapDolMemoryBlocks(
            Memory memory,
            FileBytes fileBytes,
            List<DolSection> sections,
            long bssAddr,
            long bssSize,
            long entryPoint
    ) throws Exception {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();

        // Remove blocos iniciais criados pelo BinaryLoader em 0x00000000
        MemoryBlock[] existingBlocks = memory.getBlocks();
        for (MemoryBlock block : existingBlocks) {
            memory.removeBlock(block, monitor);
        }

        // Cria blocos mapeados diretamente do FileBytes conforme o cabecalho DOL
        for (DolSection sec : sections) {
            Address startAddr = space.getAddress(sec.vaddr);
            boolean r = true;
            boolean w = !sec.isText && !".rodata".equals(sec.role) && !".sdata2".equals(sec.role);
            boolean x = sec.isText;
            MemoryBlock block = memory.createInitializedBlock(
                    sec.name + "_" + sec.role.replace(".", ""),
                    startAddr,
                    fileBytes,
                    sec.fileOffset,
                    sec.size,
                    false
            );
            block.setPermissions(r, w, x);
            block.setComment(String.format("DOL section %s (%s) file_offset=0x%08X vaddr=0x%08X size=0x%08X",
                    sec.name, sec.role, sec.fileOffset, sec.vaddr, sec.size));
        }

        // Cria regioes .sbss e .bss sem sobrepor .data7 (.sdata2 em 0x8050E7E0..0x80512D20)
        long bssEnd = bssAddr + bssSize;
        long cursor = bssAddr;
        List<DolSection> sortedByVaddr = new ArrayList<>(sections);
        sortedByVaddr.sort(Comparator.comparingLong(s -> s.vaddr));
        int bssPart = 0;
        for (DolSection sec : sortedByVaddr) {
            if (sec.endAddr() <= cursor || sec.vaddr >= bssEnd) continue;
            if (sec.vaddr > cursor) {
                long gapSize = sec.vaddr - cursor;
                String bssName = (bssPart == 0) ? ".sbss" : (".bss_" + bssPart);
                MemoryBlock bssBlock = memory.createUninitializedBlock(bssName, space.getAddress(cursor), gapSize, false);
                bssBlock.setPermissions(true, true, false);
                bssPart++;
            }
            cursor = Math.max(cursor, sec.endAddr());
        }
        if (cursor < bssEnd) {
            long remSize = bssEnd - cursor;
            String bssName = (bssPart == 0) ? ".bss" : ".bss_main";
            MemoryBlock bssBlock = memory.createUninitializedBlock(bssName, space.getAddress(cursor), remSize, false);
            bssBlock.setPermissions(true, true, false);
        }

        // Configura Entry Point e simbolos de arquitetura PowerPC EABI
        SymbolTable symTab = currentProgram.getSymbolTable();
        Address entryAddr = space.getAddress(entryPoint);
        symTab.addExternalEntryPoint(entryAddr);
        symTab.createLabel(entryAddr, "__start", SourceType.ANALYSIS);
        symTab.createLabel(space.getAddress(DEFAULT_SDA2_BASE), "_SDA2_BASE_", SourceType.ANALYSIS);
        symTab.createLabel(space.getAddress(DEFAULT_SDA_BASE), "_SDA_BASE_", SourceType.ANALYSIS);

        // Define r2 (_SDA2_BASE_) e r13 (_SDA_BASE_) no ProgramContext do Ghidra
        ProgramContext ctx = currentProgram.getProgramContext();
        Register r2 = currentProgram.getLanguage().getRegister("r2");
        Register r13 = currentProgram.getLanguage().getRegister("r13");
        for (DolSection sec : sections) {
            if (!sec.isText) continue;
            Address start = space.getAddress(sec.vaddr);
            Address end = space.getAddress(sec.endAddr() - 1);
            if (r2 != null) {
                ctx.setValue(r2, start, end, BigInteger.valueOf(DEFAULT_SDA2_BASE));
            }
            if (r13 != null) {
                ctx.setValue(r13, start, end, BigInteger.valueOf(DEFAULT_SDA_BASE));
            }
        }
    }

    private static boolean isValidDolTargetAddress(long addr, List<DolSection> sections, long bssAddr, long bssSize) {
        if (addr >= bssAddr && addr < bssAddr + bssSize) {
            return true;
        }
        for (DolSection sec : sections) {
            if (addr >= sec.vaddr && addr < sec.endAddr()) {
                return true;
            }
        }
        return false;
    }

    private static DolSection findSectionContaining(long addr, List<DolSection> sections) {
        for (DolSection sec : sections) {
            if (addr >= sec.vaddr && addr < sec.endAddr()) {
                return sec;
            }
        }
        return null;
    }

    private TreeSet<Long> recoverFunctions(List<DolSection> sections, long entryPoint, Set<Long> pointerTargets) {
        TreeSet<Long> funcs = new TreeSet<>();
        funcs.add(entryPoint);

        for (DolSection sec : sections) {
            if (!sec.isText) continue;
            funcs.add(sec.vaddr);
            ByteBuffer bb = ByteBuffer.wrap(sec.data).order(ByteOrder.BIG_ENDIAN);
            int wordCount = (int) (sec.size / 4);
            for (int i = 0; i < wordCount; i++) {
                long pc = sec.vaddr + (long) i * 4L;
                int insn = bb.getInt(i * 4);
                int op = (insn >>> 26) & 0x3F;

                // 1. Alvos de chamadas BL (Branch and Link: op=18, LK=1)
                if (op == 18 && (insn & 1) == 1) {
                    int li = insn & 0x03FFFFFC;
                    if ((li & 0x02000000) != 0) {
                        li |= 0xFC000000; // extensao de sinal de 26 bits
                    }
                    long target = ((insn & 2) != 0) ? Integer.toUnsignedLong(li) : (pc + li);
                    if (isCodeAddress(target, sections)) {
                        funcs.add(target);
                    }
                }

                // 2. Prologos padrao PowerPC EABI: stwu r1, -frame(r1) (0x9421xxxx com offset negativo)
                if ((insn & 0xFFFF8000) == 0x94218000) {
                    funcs.add(pc);
                }

                // 3. Ponteiros de funcao em .ctors / vtables apontando para instrucao valida alinhada
                if (pointerTargets.contains(pc) && i > 0) {
                    int prevInsn = bb.getInt((i - 1) * 4);
                    // Se a instrucao anterior e blr (0x4E800020), b (op=18 LK=0) ou padding (0x00000000)
                    if (prevInsn == 0x4E800020 || prevInsn == 0 || ((prevInsn >>> 26) == 18 && (prevInsn & 1) == 0)) {
                        funcs.add(pc);
                    }
                }
            }
        }
        return funcs;
    }

    private static boolean isCodeAddress(long addr, List<DolSection> sections) {
        if ((addr & 3L) != 0) return false;
        for (DolSection sec : sections) {
            if (sec.isText && addr >= sec.vaddr && addr < sec.endAddr()) {
                return true;
            }
        }
        return false;
    }

    private TreeMap<Long, ExtractedString> extractReadableStrings(
            List<DolSection> sections,
            Set<Long> pointerTargets,
            Charset sjisCharset
    ) {
        TreeMap<Long, ExtractedString> result = new TreeMap<>();

        for (DolSection sec : sections) {
            // Ignora tabelas de excecao C++ (extab / extabindex) e construtores (.ctors / .dtors) na varredura de strings
            if (!sec.isText && (sec.index <= 3)) {
                continue;
            }
            // Varre secoes de dados (.rodata, .data, .sdata, .sdata2) e texto
            byte[] buf = sec.data;
            int size = buf.length;
            int i = 0;
            while (i < size) {
                long addr = sec.vaddr + i;
                boolean precededByNullOrStart = (i == 0 || buf[i - 1] == 0);
                boolean isPtrTarget = pointerTargets.contains(addr);

                if (precededByNullOrStart || isPtrTarget) {
                    int j = i;
                    int maxLen = Math.min(size, i + 512);
                    while (j < maxLen && buf[j] != 0) {
                        j++;
                    }
                    int len = j - i;
                    int minLen = isPtrTarget ? 2 : 4;
                    if (j < size && buf[j] == 0 && len >= minLen) {
                        // Nas secoes executaveis (.text), exige alinhamento de 4 bytes e pelo menos 6 caracteres
                        if (!sec.isText || ((addr & 3L) == 0 && len >= 6)) {
                            StringClassification cls = classifyStringBytes(buf, i, len, isPtrTarget, (addr & 3L) == 0, sjisCharset);
                            if (cls != null) {
                                result.put(addr, new ExtractedString(
                                        addr,
                                        sec.fileOffset + i,
                                        sec.name + " (" + sec.role + ")",
                                        cls.encoding,
                                        len,
                                        cls.decoded
                                ));
                                i = j + 1;
                                continue;
                            }
                        }
                    }
                }
                i++;
            }
        }
        return result;
    }

    private static class StringClassification {
        final String encoding;
        final String decoded;

        StringClassification(String encoding, String decoded) {
            this.encoding = encoding;
            this.decoded = decoded;
        }
    }

    private static StringClassification classifyStringBytes(
            byte[] buf,
            int offset,
            int len,
            boolean isPtrTarget,
            boolean isAligned4,
            Charset sjisCharset
    ) {
        // Caso 1: Verifica se todos os bytes sao ASCII imprimivel
        boolean allAscii = true;
        int alnumCount = 0;
        int letterCount = 0;
        int printableCount = 0;
        for (int k = 0; k < len; k++) {
            int b = buf[offset + k] & 0xFF;
            if ((b >= 0x20 && b <= 0x7E) || b == 0x09 || b == 0x0A || b == 0x0D) {
                printableCount++;
                if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z')) {
                    alnumCount++;
                    letterCount++;
                } else if (b >= '0' && b <= '9') {
                    alnumCount++;
                }
            } else {
                allAscii = false;
                break;
            }
        }

        if (allAscii) {
            if (!isAligned4 && !isPtrTarget && len < 6) {
                return null;
            }
            if (len < 4) {
                // Para strings ASCII curtas (2..3 bytes) apontadas por ponteiro (ex: "GK", "FW", "MF", "DF", "TP", "SE", "%d")
                if (!isPtrTarget) return null;
                if (letterCount < 2 && !containsFormatSpecifier(buf, offset, len)) return null;
            } else if (!isPtrTarget) {
                if (letterCount == 0 && len < 6) return null;
                if (alnumCount * 2 < len && !containsFormatSpecifier(buf, offset, len)) return null;
                if (looksLikeFloatAsciiNoise(buf, offset, len)) return null;
            } else if (alnumCount == 0 && !containsFormatSpecifier(buf, offset, len)) {
                return null;
            }
            String s = new String(buf, offset, len, StandardCharsets.US_ASCII);
            return new StringClassification("ASCII", s);
        }

        // Caso 2: Verifica Shift-JIS estrito (Hiragana, Katakana, Kanji e pontuacao japonesa da Level-5)
        if (!isAligned4 && !isPtrTarget) {
            return null;
        }
        if (looksLikeFloatTableBytes(buf, offset, len)) {
            return null;
        }

        int idx = 0;
        int jpChars = 0;
        int asciiChars = 0;
        while (idx < len) {
            int b1 = buf[offset + idx] & 0xFF;
            if ((b1 >= 0x20 && b1 <= 0x7E) || b1 == 0x09 || b1 == 0x0A || b1 == 0x0D) {
                asciiChars++;
                idx++;
            } else if (((b1 >= 0x81 && b1 <= 0x9F) || (b1 >= 0xE0 && b1 <= 0xEA)) && (idx + 1 < len)) {
                int b2 = buf[offset + idx + 1] & 0xFF;
                if (!isValidStrictShiftJisPair(b1, b2)) {
                    return null;
                }
                jpChars++;
                idx += 2;
            } else {
                return null;
            }
        }

        int minJpChars = isPtrTarget ? 1 : 2;
        if (jpChars >= minJpChars && (jpChars * 2 >= (jpChars + asciiChars) || isPtrTarget)) {
            String decoded = new String(buf, offset, len, sjisCharset);
            if (!decoded.contains("\uFFFD")) {
                return new StringClassification("Shift-JIS", decoded);
            }
        }

        return null;
    }

    private static boolean containsFormatSpecifier(byte[] buf, int offset, int len) {
        for (int i = 0; i + 1 < len; i++) {
            if (buf[offset + i] == '%') {
                int next = buf[offset + i + 1] & 0xFF;
                if (next == 's' || next == 'd' || next == 'x' || next == 'X' || next == 'f' || next == 'p' || next == 'u' || (next >= '0' && next <= '9')) {
                    return true;
                }
            }
        }
        return false;
    }

    private static boolean looksLikeFloatAsciiNoise(byte[] buf, int offset, int len) {
        if (len == 4) {
            int b0 = buf[offset] & 0xFF;
            int b1 = buf[offset + 1] & 0xFF;
            int b2 = buf[offset + 2] & 0xFF;
            int b3 = buf[offset + 3] & 0xFF;
            // Padroes tipicos de mantissa repetida em IEEE-754 (0x333333, 0x555555, 0x666666) ou bytes identicos
            if (b1 == b2 && b2 == b3) return true;
            if (b0 == b1 && b1 == b2) return true;
        }
        return false;
    }

    private static boolean looksLikeFloatTableBytes(byte[] buf, int offset, int len) {
        // Verifica se palavras alinhadas contem padroes classicos de floats IEEE-754 Big-Endian (0x3B..0x43 seguido de 0x33, 0xCD, 0xAA, 0xD7)
        for (int i = 0; i + 1 < len; i++) {
            int b0 = buf[offset + i] & 0xFF;
            int b1 = buf[offset + i + 1] & 0xFF;
            if (b0 >= 0x3B && b0 <= 0x43 && (b1 == 0x23 || b1 == 0x33 || b1 == 0x4C || b1 == 0x66 || b1 == 0x80 || b1 == 0xAA || b1 == 0xCC)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isValidStrictShiftJisPair(int b1, int b2) {
        if (b2 < 0x40 || b2 == 0x7F || b2 > 0xFC) return false;
        if (b1 == 0x81) {
            return b2 >= 0x40 && b2 <= 0xAC; // Pontuacao japonesa comum
        }
        if (b1 == 0x82) {
            // Digitos full-width (0x4F..0x58), Latim full-width (0x60..0x9A) e Hiragana (0x9F..0xF1)
            return (b2 >= 0x4F && b2 <= 0x58)
                    || (b2 >= 0x60 && b2 <= 0x79)
                    || (b2 >= 0x81 && b2 <= 0x9A)
                    || (b2 >= 0x9F && b2 <= 0xF1);
        }
        if (b1 == 0x83) {
            // Katakana (0x40..0x96)
            return b2 >= 0x40 && b2 <= 0x96;
        }
        if (b1 >= 0x88 && b1 <= 0x9F) {
            // Kanji JIS X 0208 Nivel 1
            return true;
        }
        if (b1 >= 0xE0 && b1 <= 0xEA) {
            // Kanji JIS X 0208 Nivel 2
            return true;
        }
        return false;
    }

    private TreeMap<Long, PointerTable> extractPointerTables(
            List<DolSection> sections,
            Map<Long, Long> dataPointerMap,
            TreeMap<Long, ExtractedString> stringMap,
            TreeSet<Long> functionStarts,
            long bssAddr,
            long bssSize
    ) {
        TreeMap<Long, PointerTable> tables = new TreeMap<>();

        for (DolSection sec : sections) {
            if (sec.isText || sec.index == 0 || sec.index == 1) continue;
            int wordCount = (int) (sec.size / 4);
            int i = 0;
            while (i < wordCount) {
                long startSlot = sec.vaddr + (long) i * 4L;
                if (!dataPointerMap.containsKey(startSlot)) {
                    i++;
                    continue;
                }

                // Coleta sequencia contigua de ponteiros validos de 32 bits Big-Endian
                int j = i;
                while (j < wordCount && dataPointerMap.containsKey(sec.vaddr + (long) j * 4L)) {
                    j++;
                }

                int count = j - i;
                if (count >= 2) {
                    int strTargets = 0;
                    int codeTargets = 0;
                    int dataTargets = 0;
                    List<PointerTableEntry> entries = new ArrayList<>(count);

                    for (int k = 0; k < count; k++) {
                        long entryAddr = sec.vaddr + (long) (i + k) * 4L;
                        long targetAddr = dataPointerMap.get(entryAddr);
                        DolSection targetSec = findSectionContaining(targetAddr, sections);
                        String targetSecName = (targetSec != null)
                                ? (targetSec.name + " (" + targetSec.role + ")")
                                : ((targetAddr >= bssAddr && targetAddr < bssAddr + bssSize) ? ".bss" : "unknown");

                        ExtractedString targetStr = stringMap.get(targetAddr);
                        String kind;
                        String preview = null;
                        if (targetStr != null) {
                            kind = "string";
                            preview = targetStr.value;
                            strTargets++;
                        } else if (targetSec != null && targetSec.isText) {
                            kind = functionStarts.contains(targetAddr) ? "function" : "code";
                            preview = String.format("FUN_%08X", targetAddr);
                            codeTargets++;
                        } else {
                            kind = "data";
                            dataTargets++;
                        }
                        entries.add(new PointerTableEntry(k, entryAddr, targetAddr, targetSecName, kind, preview));
                    }

                    String category;
                    if (strTargets == count) {
                        category = "string_pointer_table";
                    } else if (strTargets > 0 && strTargets * 2 >= count) {
                        category = "string_rich_pointer_table";
                    } else if (codeTargets == count) {
                        category = ".ctors".equals(sec.role) ? "ctors_function_table" : "function_pointer_table_or_vtable";
                    } else if (codeTargets > 0 && dataTargets > 0 && strTargets == 0) {
                        category = "vtable_or_callback_struct_table";
                    } else if (strTargets > 0) {
                        category = "mixed_string_data_table";
                    } else {
                        category = "data_pointer_table";
                    }

                    long fileOff = sec.fileOffset + (long) i * 4L;
                    PointerTable pt = new PointerTable(
                            startSlot,
                            fileOff,
                            sec.name + " (" + sec.role + ")",
                            category,
                            count,
                            count * 4
                    );
                    pt.entries.addAll(entries);
                    tables.put(startSlot, pt);
                }

                i = j;
            }
        }
        return tables;
    }

    private int[] analyzePowerPcCodeXrefs(
            List<DolSection> sections,
            TreeSet<Long> functionStarts,
            Map<Long, Long> dataPointerMap,
            TreeMap<Long, ExtractedString> stringMap,
            TreeMap<Long, PointerTable> pointerTableMap,
            Map<Long, PointerTable> slotToTableMap,
            long minVaddr,
            long maxVaddr
    ) {
        int strCodeXrefCount = 0;
        int tableCodeXrefCount = 0;
        int allCodeRefCount = 0;

        long[] regVal = new long[32];
        boolean[] regValid = new boolean[32];

        for (DolSection sec : sections) {
            if (!sec.isText) continue;
            ByteBuffer bb = ByteBuffer.wrap(sec.data).order(ByteOrder.BIG_ENDIAN);
            int wordCount = (int) (sec.size / 4);

            resetRegisters(regVal, regValid);
            long currentFuncAddr = sec.vaddr;

            for (int i = 0; i < wordCount; i++) {
                long pc = sec.vaddr + (long) i * 4L;
                if (functionStarts.contains(pc)) {
                    currentFuncAddr = pc;
                    resetRegisters(regVal, regValid);
                }

                int insn = bb.getInt(i * 4);
                int op = (insn >>> 26) & 0x3F;
                int rd = (insn >>> 21) & 0x1F;
                int ra = (insn >>> 16) & 0x1F;
                int imm = insn & 0xFFFF;
                int simm = (short) imm;

                if (op == 15) {
                    // addis / lis rD, rA, SIMM16
                    if (ra == 0) {
                        regVal[rd] = Integer.toUnsignedLong(imm << 16);
                        regValid[rd] = true;
                    } else if (regValid[ra]) {
                        regVal[rd] = (regVal[ra] + ((long) simm << 16)) & 0xFFFFFFFFL;
                        regValid[rd] = true;
                    } else if (rd != 2 && rd != 13) {
                        regValid[rd] = false;
                    }
                } else if (op == 14) {
                    // addi / li rD, rA, SIMM16
                    if (ra == 0) {
                        if (rd != 2 && rd != 13) {
                            regValid[rd] = false;
                        }
                    } else if (regValid[ra]) {
                        long target = (regVal[ra] + simm) & 0xFFFFFFFFL;
                        String refKind = (ra == 2 || ra == 13)
                                ? ("SDA_ADDI_r" + ra)
                                : ((regVal[ra] & 0xFFFFL) == 0 ? "HI_LO_ADDI" : "BASE_REL_ADDI");
                        if (rd != 2 && rd != 13) {
                            regVal[rd] = target;
                            regValid[rd] = true;
                        }
                        if (target >= minVaddr && target < maxVaddr) {
                            allCodeRefCount++;
                            int[] delta = recordCodeXref(
                                    pc,
                                    currentFuncAddr,
                                    target,
                                    refKind,
                                    "addi",
                                    rd,
                                    null,
                                    stringMap,
                                    pointerTableMap,
                                    slotToTableMap
                            );
                            strCodeXrefCount += delta[0];
                            tableCodeXrefCount += delta[1];
                        }
                    } else if (rd != 2 && rd != 13) {
                        regValid[rd] = false;
                    }
                } else if (op == 24) {
                    // ori rA_dest, rS_src, UIMM16 (em PowerPC ori: bits 21..25 = rS, bits 16..20 = rA_dest)
                    int rs = rd;
                    int raDest = ra;
                    if (rs == raDest && imm == 0) {
                        // nop (ori r0, r0, 0)
                        continue;
                    }
                    if (regValid[rs] && rs != 0) {
                        long target = (regVal[rs] | imm) & 0xFFFFFFFFL;
                        if (raDest != 2 && raDest != 13) {
                            regVal[raDest] = target;
                            regValid[raDest] = true;
                        }
                        if (imm != 0 && target >= minVaddr && target < maxVaddr) {
                            allCodeRefCount++;
                            int[] delta = recordCodeXref(
                                    pc,
                                    currentFuncAddr,
                                    target,
                                    "HI_ORI",
                                    "ori",
                                    raDest,
                                    null,
                                    stringMap,
                                    pointerTableMap,
                                    slotToTableMap
                            );
                            strCodeXrefCount += delta[0];
                            tableCodeXrefCount += delta[1];
                        }
                    } else if (raDest != 2 && raDest != 13) {
                        regValid[raDest] = false;
                    }
                } else if (isLoadOrStoreOpcode(op)) {
                    String mnem = getLoadStoreMnemonic(op);
                    boolean isLoad = isLoadOpcode(op);
                    if (ra != 0 && regValid[ra]) {
                        long effectiveAddr = (regVal[ra] + simm) & 0xFFFFFFFFL;
                        if (effectiveAddr >= minVaddr && effectiveAddr < maxVaddr) {
                            allCodeRefCount++;
                            String refKind = (ra == 2 || ra == 13) ? ("SDA_MEM_r" + ra) : "HI_LO_MEM";
                            int[] delta = recordCodeXref(
                                    pc,
                                    currentFuncAddr,
                                    effectiveAddr,
                                    refKind,
                                    mnem,
                                    rd,
                                    null,
                                    stringMap,
                                    pointerTableMap,
                                    slotToTableMap
                            );
                            strCodeXrefCount += delta[0];
                            tableCodeXrefCount += delta[1];

                            // Se for lwz (op=32) lendo um ponteiro constante de .sdata/.sdata2/.data/.rodata,
                            // propaga o endereco apontado e registra XREF indireta para string ou tabela!
                            if (op == 32 && dataPointerMap.containsKey(effectiveAddr)) {
                                long pointedTarget = dataPointerMap.get(effectiveAddr);
                                if (rd != 2 && rd != 13) {
                                    regVal[rd] = pointedTarget;
                                    regValid[rd] = true;
                                }
                                int[] indDelta = recordCodeXref(
                                        pc,
                                        currentFuncAddr,
                                        pointedTarget,
                                        "INDIRECT_PTR_LOAD",
                                        mnem,
                                        rd,
                                        effectiveAddr,
                                        stringMap,
                                        pointerTableMap,
                                        slotToTableMap
                                );
                                strCodeXrefCount += indDelta[0];
                                tableCodeXrefCount += indDelta[1];
                            } else if (isLoad && rd != 2 && rd != 13) {
                                regValid[rd] = false;
                            }
                        } else if (isLoad && rd != 2 && rd != 13) {
                            regValid[rd] = false;
                        }
                    } else if (isLoad && rd != 2 && rd != 13) {
                        regValid[rd] = false;
                    }
                } else if (op == 18 && (insn & 1) == 1) {
                    // Chamada de sub-rotina (bl) invalida registradores volateis EABI (r0, r3..r12),
                    // preservando registradores nao-volateis (r14..r31) e bases SDA (r2, r13)
                    regValid[0] = false;
                    for (int r = 3; r <= 12; r++) {
                        regValid[r] = false;
                    }
                } else if (op == 19 && ((insn >>> 1) & 0x3FF) == 16) {
                    // blr (retorno de funcao)
                    resetRegisters(regVal, regValid);
                }
            }
        }

        return new int[]{strCodeXrefCount, tableCodeXrefCount, allCodeRefCount};
    }

    private static void resetRegisters(long[] regVal, boolean[] regValid) {
        for (int i = 0; i < 32; i++) {
            regVal[i] = 0L;
            regValid[i] = false;
        }
        regVal[1] = DEFAULT_STACK_ADDR;
        regValid[1] = true;
        regVal[2] = DEFAULT_SDA2_BASE;
        regValid[2] = true;
        regVal[13] = DEFAULT_SDA_BASE;
        regValid[13] = true;
    }

    private static boolean isLoadOrStoreOpcode(int op) {
        return op == 32 || op == 33 || op == 34 || op == 35
                || op == 36 || op == 37 || op == 38 || op == 39
                || op == 40 || op == 41 || op == 42 || op == 43
                || op == 44 || op == 45 || op == 48 || op == 50 || op == 52 || op == 54;
    }

    private static boolean isLoadOpcode(int op) {
        return op == 32 || op == 33 || op == 34 || op == 35
                || op == 40 || op == 41 || op == 42 || op == 43
                || op == 48 || op == 50;
    }

    private static String getLoadStoreMnemonic(int op) {
        switch (op) {
            case 32: return "lwz";
            case 33: return "lwzu";
            case 34: return "lbz";
            case 35: return "lbzu";
            case 36: return "stw";
            case 37: return "stwu";
            case 38: return "stb";
            case 39: return "stbu";
            case 40: return "lhz";
            case 41: return "lhzu";
            case 42: return "lha";
            case 43: return "lhau";
            case 44: return "sth";
            case 45: return "sthu";
            case 48: return "lfs";
            case 50: return "lfd";
            case 52: return "stfs";
            case 54: return "stfd";
            default: return "mem";
        }
    }

    private static String getPowerPcEabiRole(int reg) {
        if (reg >= 3 && reg <= 10) {
            int argIdx = reg - 2;
            if (reg == 3 || reg == 4) {
                return "EABI_arg" + argIdx + "_or_return (r" + reg + ")";
            }
            return "EABI_arg" + argIdx + " (r" + reg + ")";
        }
        if (reg == 1) return "EABI_stack_pointer (r1)";
        if (reg == 2) return "EABI_SDA2_base (r2)";
        if (reg == 13) return "EABI_SDA_base (r13)";
        if (reg == 0 || reg == 11 || reg == 12) return "EABI_volatile_scratch (r" + reg + ")";
        return "EABI_callee_saved_nonvolatile (r" + reg + ")";
    }

    private int[] recordCodeXref(
            long fromAddr,
            long funcAddr,
            long targetAddr,
            String refType,
            String mnemonic,
            int reg,
            Long viaPointerAddr,
            TreeMap<Long, ExtractedString> stringMap,
            TreeMap<Long, PointerTable> pointerTableMap,
            Map<Long, PointerTable> slotToTableMap
    ) {
        int strAdded = 0;
        int tableAdded = 0;
        String funcName = String.format("FUN_%08X", funcAddr);
        String regName = "r" + reg;
        String eabiRole = getPowerPcEabiRole(reg);

        ExtractedString str = stringMap.get(targetAddr);
        if (str != null) {
            str.codeXrefs.add(new CodeXref(
                    fromAddr,
                    funcAddr,
                    funcName,
                    targetAddr,
                    refType,
                    mnemonic,
                    regName,
                    eabiRole,
                    viaPointerAddr
            ));
            strAdded++;
        }

        PointerTable pt = pointerTableMap.get(targetAddr);
        if (pt == null && viaPointerAddr == null) {
            pt = slotToTableMap.get(targetAddr);
        }
        if (pt != null) {
            pt.codeXrefs.add(new CodeXref(
                    fromAddr,
                    funcAddr,
                    funcName,
                    targetAddr,
                    refType,
                    mnemonic,
                    regName,
                    eabiRole,
                    viaPointerAddr
            ));
            tableAdded++;
        }

        return new int[]{strAdded, tableAdded};
    }

    private void populateGhidraSymbolsAndReferences(
            TreeMap<Long, ExtractedString> stringMap,
            TreeMap<Long, PointerTable> pointerTableMap
    ) {
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        SymbolTable symTab = currentProgram.getSymbolTable();
        ReferenceManager refMgr = currentProgram.getReferenceManager();

        int addedStrLabels = 0;
        for (ExtractedString s : stringMap.values()) {
            if (s.codeXrefs.isEmpty() && s.dataXrefs.isEmpty()) continue;
            try {
                Address strAddr = space.getAddress(s.address);
                if (addedStrLabels < 2500) {
                    symTab.createLabel(strAddr, String.format("STR_%08X", s.address), SourceType.ANALYSIS);
                    addedStrLabels++;
                }
                for (CodeXref cx : s.codeXrefs) {
                    refMgr.addMemoryReference(space.getAddress(cx.fromAddr), strAddr, RefType.DATA, SourceType.ANALYSIS, 0);
                }
            } catch (Exception ignored) {
            }
        }

        int addedTblLabels = 0;
        for (PointerTable pt : pointerTableMap.values()) {
            if (pt.codeXrefs.isEmpty() && pt.dataXrefs.isEmpty() && !"string_pointer_table".equals(pt.category)) continue;
            try {
                Address tblAddr = space.getAddress(pt.address);
                if (addedTblLabels < 1500) {
                    symTab.createLabel(tblAddr, String.format("PTR_TBL_%08X", pt.address), SourceType.ANALYSIS);
                    addedTblLabels++;
                }
                for (CodeXref cx : pt.codeXrefs) {
                    refMgr.addMemoryReference(space.getAddress(cx.fromAddr), tblAddr, RefType.DATA, SourceType.ANALYSIS, 0);
                }
            } catch (Exception ignored) {
            }
        }
    }

    private void writeJsonReport(
            File outFile,
            long totalFileSize,
            long entryPoint,
            long bssAddr,
            long bssSize,
            List<DolSection> sections,
            int recoveredFunctionCount,
            int totalAllCodeRefs,
            int totalCodeXrefsToStrings,
            int totalDataXrefsToStrings,
            int totalCodeXrefsToTables,
            int totalDataXrefsToTables,
            TreeMap<Long, ExtractedString> stringMap,
            TreeMap<Long, PointerTable> pointerTableMap
    ) throws Exception {
        int referencedStringsCount = 0;
        int sjisStringsCount = 0;
        for (ExtractedString s : stringMap.values()) {
            if (!s.codeXrefs.isEmpty() || !s.dataXrefs.isEmpty()) {
                referencedStringsCount++;
            }
            if ("Shift-JIS".equals(s.encoding)) {
                sjisStringsCount++;
            }
        }

        int referencedTablesCount = 0;
        Map<String, Integer> tableCategoryCounts = new LinkedHashMap<>();
        for (PointerTable pt : pointerTableMap.values()) {
            if (!pt.codeXrefs.isEmpty() || !pt.dataXrefs.isEmpty()) {
                referencedTablesCount++;
            }
            tableCategoryCounts.put(pt.category, tableCategoryCounts.getOrDefault(pt.category, 0) + 1);
        }

        try (BufferedWriter w = new BufferedWriter(
                new OutputStreamWriter(new FileOutputStream(outFile), StandardCharsets.UTF_8), 262144)) {

            w.write("{\n");
            w.write("  \"metadata\": {\n");
            w.write("    \"game\": \"Inazuma Eleven GO Strikers 2013\",\n");
            w.write("    \"binary\": \"extracted/files/sys/main.dol\",\n");
            w.write(String.format("    \"binary_size_bytes\": %d,\n", totalFileSize));
            w.write("    \"processor\": \"PowerPC 750CL (Broadway) 32-bit Big-Endian\",\n");
            w.write(String.format("    \"ghidra_language_id\": \"%s\",\n", escapeJson(currentProgram.getLanguageID().getIdAsString())));
            w.write(String.format("    \"ghidra_version\": \"%s\",\n", escapeJson(currentProgram.getMetadata().getOrDefault("Created With Ghidra Version", "12.1.4"))));
            w.write("    \"calling_convention\": \"PowerPC EABI (args: r3-r10, returns: r3/r4, SDA2: r2, SDA: r13)\",\n");
            w.write(String.format("    \"entry_point\": \"0x%08X\",\n", entryPoint));
            w.write(String.format("    \"bss_address\": \"0x%08X\",\n", bssAddr));
            w.write(String.format("    \"bss_size\": \"0x%08X\",\n", bssSize));
            w.write(String.format("    \"stack_pointer_init_r1\": \"0x%08X\",\n", DEFAULT_STACK_ADDR));
            w.write(String.format("    \"sda2_base_r2\": \"0x%08X\",\n", DEFAULT_SDA2_BASE));
            w.write(String.format("    \"sda_base_r13\": \"0x%08X\"\n", DEFAULT_SDA_BASE));
            w.write("  },\n");

            // Sections
            w.write("  \"dol_sections\": [\n");
            for (int i = 0; i < sections.size(); i++) {
                DolSection sec = sections.get(i);
                w.write("    {\n");
                w.write(String.format("      \"name\": \"%s\",\n", sec.name));
                w.write(String.format("      \"role\": \"%s\",\n", sec.role));
                w.write(String.format("      \"type\": \"%s\",\n", sec.isText ? "CODE" : "DATA"));
                w.write(String.format("      \"file_offset\": \"0x%08X\",\n", sec.fileOffset));
                w.write(String.format("      \"virtual_address\": \"0x%08X\",\n", sec.vaddr));
                w.write(String.format("      \"end_address\": \"0x%08X\",\n", sec.endAddr()));
                w.write(String.format("      \"size_bytes\": %d\n", sec.size));
                w.write(i + 1 < sections.size() ? "    },\n" : "    }\n");
            }
            w.write("  ],\n");

            // Summary statistics
            w.write("  \"summary\": {\n");
            w.write(String.format("    \"recovered_functions_count\": %d,\n", recoveredFunctionCount));
            w.write(String.format("    \"total_code_address_references\": %d,\n", totalAllCodeRefs));
            w.write(String.format("    \"total_strings_extracted\": %d,\n", stringMap.size()));
            w.write(String.format("    \"ascii_strings_count\": %d,\n", stringMap.size() - sjisStringsCount));
            w.write(String.format("    \"shift_jis_strings_count\": %d,\n", sjisStringsCount));
            w.write(String.format("    \"strings_with_xrefs_count\": %d,\n", referencedStringsCount));
            w.write(String.format("    \"total_code_xrefs_to_strings\": %d,\n", totalCodeXrefsToStrings));
            w.write(String.format("    \"total_data_xrefs_to_strings\": %d,\n", totalDataXrefsToStrings));
            w.write(String.format("    \"total_pointer_tables_extracted\": %d,\n", pointerTableMap.size()));
            w.write(String.format("    \"pointer_tables_with_xrefs_count\": %d,\n", referencedTablesCount));
            w.write(String.format("    \"total_code_xrefs_to_pointer_tables\": %d,\n", totalCodeXrefsToTables));
            w.write(String.format("    \"total_data_xrefs_to_pointer_tables\": %d,\n", totalDataXrefsToTables));
            w.write("    \"pointer_table_categories\": {\n");
            int catIdx = 0;
            for (Map.Entry<String, Integer> entry : tableCategoryCounts.entrySet()) {
                catIdx++;
                w.write(String.format("      \"%s\": %d%s\n",
                        escapeJson(entry.getKey()),
                        entry.getValue(),
                        catIdx < tableCategoryCounts.size() ? "," : ""));
            }
            w.write("    }\n");
            w.write("  },\n");

            // Strings list
            w.write("  \"strings\": [\n");
            int strIdx = 0;
            int totalStrings = stringMap.size();
            for (ExtractedString s : stringMap.values()) {
                strIdx++;
                w.write("    {\n");
                w.write(String.format("      \"address\": \"0x%08X\",\n", s.address));
                w.write(String.format("      \"file_offset\": \"0x%08X\",\n", s.fileOffset));
                w.write(String.format("      \"section\": \"%s\",\n", escapeJson(s.sectionName)));
                w.write(String.format("      \"encoding\": \"%s\",\n", escapeJson(s.encoding)));
                w.write(String.format("      \"byte_length\": %d,\n", s.byteLength));
                w.write(String.format("      \"value\": \"%s\",\n", escapeJson(s.value)));
                w.write(String.format("      \"xref_count\": %d,\n", s.codeXrefs.size() + s.dataXrefs.size()));

                w.write("      \"code_xrefs\": [");
                if (!s.codeXrefs.isEmpty()) {
                    w.write("\n");
                    for (int k = 0; k < s.codeXrefs.size(); k++) {
                        CodeXref cx = s.codeXrefs.get(k);
                        w.write("        {");
                        w.write(String.format("\"from_address\": \"0x%08X\", ", cx.fromAddr));
                        w.write(String.format("\"function_entry\": \"0x%08X\", ", cx.functionAddr));
                        w.write(String.format("\"function_name\": \"%s\", ", cx.functionName));
                        w.write(String.format("\"type\": \"%s\", ", cx.refType));
                        w.write(String.format("\"mnemonic\": \"%s\", ", cx.mnemonic));
                        w.write(String.format("\"register\": \"%s\", ", cx.registerName));
                        w.write(String.format("\"eabi_role\": \"%s\"", cx.eabiRole));
                        if (cx.viaPointerAddr != null) {
                            w.write(String.format(", \"via_pointer_slot\": \"0x%08X\"", cx.viaPointerAddr));
                        }
                        w.write(k + 1 < s.codeXrefs.size() ? "},\n" : "}\n");
                    }
                    w.write("      ");
                }
                w.write("],\n");

                w.write("      \"data_xrefs\": [");
                if (!s.dataXrefs.isEmpty()) {
                    w.write("\n");
                    for (int k = 0; k < s.dataXrefs.size(); k++) {
                        DataXref dx = s.dataXrefs.get(k);
                        w.write("        {");
                        w.write(String.format("\"from_address\": \"0x%08X\", ", dx.fromAddr));
                        w.write(String.format("\"from_section\": \"%s\"", dx.fromSection));
                        if (dx.tableAddr != null) {
                            w.write(String.format(", \"pointer_table\": \"0x%08X\", \"table_index\": %d", dx.tableAddr, dx.tableIndex));
                        }
                        w.write(k + 1 < s.dataXrefs.size() ? "},\n" : "}\n");
                    }
                    w.write("      ");
                }
                w.write("]\n");

                w.write(strIdx < totalStrings ? "    },\n" : "    }\n");
            }
            w.write("  ],\n");

            // Pointer tables list
            w.write("  \"pointer_tables\": [\n");
            int tblIdx = 0;
            int totalTables = pointerTableMap.size();
            for (PointerTable pt : pointerTableMap.values()) {
                tblIdx++;
                w.write("    {\n");
                w.write(String.format("      \"address\": \"0x%08X\",\n", pt.address));
                w.write(String.format("      \"file_offset\": \"0x%08X\",\n", pt.fileOffset));
                w.write(String.format("      \"section\": \"%s\",\n", escapeJson(pt.sectionName)));
                w.write(String.format("      \"category\": \"%s\",\n", escapeJson(pt.category)));
                w.write(String.format("      \"entry_count\": %d,\n", pt.entryCount));
                w.write(String.format("      \"byte_size\": %d,\n", pt.byteSize));
                w.write(String.format("      \"xref_count\": %d,\n", pt.codeXrefs.size() + pt.dataXrefs.size()));

                w.write("      \"code_xrefs\": [");
                if (!pt.codeXrefs.isEmpty()) {
                    w.write("\n");
                    for (int k = 0; k < pt.codeXrefs.size(); k++) {
                        CodeXref cx = pt.codeXrefs.get(k);
                        w.write("        {");
                        w.write(String.format("\"from_address\": \"0x%08X\", ", cx.fromAddr));
                        w.write(String.format("\"function_entry\": \"0x%08X\", ", cx.functionAddr));
                        w.write(String.format("\"function_name\": \"%s\", ", cx.functionName));
                        w.write(String.format("\"target_address\": \"0x%08X\", ", cx.targetAddr));
                        w.write(String.format("\"type\": \"%s\", ", cx.refType));
                        w.write(String.format("\"mnemonic\": \"%s\", ", cx.mnemonic));
                        w.write(String.format("\"register\": \"%s\", ", cx.registerName));
                        w.write(String.format("\"eabi_role\": \"%s\"", cx.eabiRole));
                        if (cx.viaPointerAddr != null) {
                            w.write(String.format(", \"via_pointer_slot\": \"0x%08X\"", cx.viaPointerAddr));
                        }
                        w.write(k + 1 < pt.codeXrefs.size() ? "},\n" : "}\n");
                    }
                    w.write("      ");
                }
                w.write("],\n");

                w.write("      \"data_xrefs\": [");
                if (!pt.dataXrefs.isEmpty()) {
                    w.write("\n");
                    for (int k = 0; k < pt.dataXrefs.size(); k++) {
                        DataXref dx = pt.dataXrefs.get(k);
                        w.write("        {");
                        w.write(String.format("\"from_address\": \"0x%08X\", ", dx.fromAddr));
                        w.write(String.format("\"from_section\": \"%s\"", dx.fromSection));
                        if (dx.tableAddr != null) {
                            w.write(String.format(", \"pointer_table\": \"0x%08X\", \"table_index\": %d", dx.tableAddr, dx.tableIndex));
                        }
                        w.write(k + 1 < pt.dataXrefs.size() ? "},\n" : "}\n");
                    }
                    w.write("      ");
                }
                w.write("],\n");

                int maxPreviewEntries = Math.min(pt.entries.size(), 64);
                w.write("      \"entries\": [\n");
                for (int k = 0; k < maxPreviewEntries; k++) {
                    PointerTableEntry e = pt.entries.get(k);
                    w.write("        {");
                    w.write(String.format("\"index\": %d, ", e.index));
                    w.write(String.format("\"slot_address\": \"0x%08X\", ", e.entryAddr));
                    w.write(String.format("\"target_address\": \"0x%08X\", ", e.targetAddr));
                    w.write(String.format("\"target_section\": \"%s\", ", escapeJson(e.targetSection)));
                    w.write(String.format("\"target_kind\": \"%s\"", escapeJson(e.targetKind)));
                    if (e.targetPreview != null) {
                        w.write(String.format(", \"target_preview\": \"%s\"", escapeJson(e.targetPreview)));
                    }
                    w.write(k + 1 < maxPreviewEntries ? "},\n" : "}\n");
                }
                w.write("      ]");
                if (pt.entries.size() > maxPreviewEntries) {
                    w.write(String.format(",\n      \"entries_truncated\": true,\n      \"entries_omitted_count\": %d\n",
                            pt.entries.size() - maxPreviewEntries));
                } else {
                    w.write("\n");
                }

                w.write(tblIdx < totalTables ? "    },\n" : "    }\n");
            }
            w.write("  ]\n");
            w.write("}\n");
        }
    }

    private static String escapeJson(String s) {
        if (s == null) return "";
        StringBuilder sb = new StringBuilder(s.length() + 16);
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"':  sb.append("\\\""); break;
                case '\\': sb.append("\\\\"); break;
                case '\b': sb.append("\\b"); break;
                case '\f': sb.append("\\f"); break;
                case '\n': sb.append("\\n"); break;
                case '\r': sb.append("\\r"); break;
                case '\t': sb.append("\\t"); break;
                default:
                    if (c < 0x20) {
                        sb.append(String.format("\\u%04x", (int) c));
                    } else {
                        sb.append(c);
                    }
                    break;
            }
        }
        return sb.toString();
    }
}
