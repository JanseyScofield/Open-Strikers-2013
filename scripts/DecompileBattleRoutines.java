// Ghidra Headless Script (@static-analyst) para descompilar e exportar as rotinas
// matematicas de confronto (Duelo 1v1, Chute, Defesa GK, Elementos, Nivel de Hissatsu e RNG)
// de Inazuma Eleven GO Strikers 2013 (main.dol).
//@category Strikers2013.StaticAnalysis
//@author @static-analyst

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

public class DecompileBattleRoutines extends GhidraScript {

    // Intervalos de enderecos virtuais em .text1 contendo as rotinas de confronto e calculo de atributos
    private static final long[][] TARGET_RANGES = new long[][]{
        {0x800410E8L, 0x80041340L}, // Unpack de idx=9 (Atributos Base/Max e Loadout Hissatsu)
        {0x80075600L, 0x80076100L}, // Confronto 1v1 normal (Tackle, Charge, Sliding, Falta, Roubo de bola)
        {0x8007B400L, 0x8007B600L}, // Helper de diferenca de nivel / estado de chute
        {0x8007C400L, 0x8007C550L}, // Helper de bonus Keshin / Mixi-Max / Armed
        {0x8008EF20L, 0x8008F000L}, // Wrapper de RNG (Status)
        {0x80092600L, 0x80092C50L}, // Inicializacao da struct de partida (0x4C bytes) e calculo de atributo efetivo (0x80092B2C)
        {0x800999B0L, 0x80099C00L}, // Leitura/escala dos 6 atributos (Kick, Body, Control, Guard, Speed, Catch)
        {0x8009E360L, 0x8009F580L}, // Motor central de Confrontos: Chute, Shoot Block, Defesa GK, Duelo Hissatsu 1v1, Variancia e Elementos
        {0x800AF2F0L, 0x800AFBF0L}, // Calculo de distancia ao gol e posicionamento
        {0x800B7C00L, 0x800B7D50L}, // Selecao de Hissatsu ativa
        {0x800B8BF0L, 0x800B8D00L}, // Verificacao de evolucao de Hissatsu (base_power vs max_power)
        {0x80330D00L, 0x80330DF0L}  // Gerador congruente linear / PRNG base
    };

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String outJsonPath = (args != null && args.length > 0 && !args[0].isEmpty())
                ? args[0]
                : "output/symbols/battle_routines_decompiled.json";

        println("[ DecompileBattleRoutines ] Desmontando e criando funcoes nos intervalos de confronto...");

        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Listing listing = currentProgram.getListing();
        FunctionManager funcMgr = currentProgram.getFunctionManager();

        TreeSet<Long> funcStarts = new TreeSet<>();

        for (long[] range : TARGET_RANGES) {
            long start = range[0];
            long end = range[1];
            Address startAddr = space.getAddress(start);
            Address endAddr = space.getAddress(end - 1);
            AddressSet set = new AddressSet(startAddr, endAddr);

            DisassembleCommand disCmd = new DisassembleCommand(startAddr, set, true);
            disCmd.applyTo(currentProgram, monitor);

            // Detecta inicios de funcoes no intervalo (stwu r1, -N(r1) ou instrucoes logo apos blr)
            funcStarts.add(start);
            for (long addr = start; addr < end; addr += 4) {
                Address cur = space.getAddress(addr);
                int word = currentProgram.getMemory().getInt(cur);
                // stwu r1, -frame(r1)
                if ((word & 0xFFFF8000) == 0x94218000) {
                    funcStarts.add(addr);
                }
                // blr (0x4E800020) -> proxima instrucao nao-nula inicia funcao
                if (word == 0x4E800020 && addr + 4 < end) {
                    int nextWord = currentProgram.getMemory().getInt(space.getAddress(addr + 4));
                    if (nextWord != 0) {
                        funcStarts.add(addr + 4);
                    }
                }
            }
        }

        // Cria funcoes no Ghidra FunctionManager
        List<Function> targetFunctions = new ArrayList<>();
        List<Long> startsList = new ArrayList<>(funcStarts);
        for (int i = 0; i < startsList.size(); i++) {
            long fStart = startsList.get(i);
            Address entry = space.getAddress(fStart);
            Function fn = funcMgr.getFunctionAt(entry);
            if (fn == null) {
                CreateFunctionCmd cmd = new CreateFunctionCmd(entry);
                cmd.applyTo(currentProgram, monitor);
                fn = funcMgr.getFunctionAt(entry);
            }
            if (fn != null) {
                targetFunctions.add(fn);
            }
        }

        println(String.format("[ DecompileBattleRoutines ] Total de funcoes prontas para descompilacao: %d", targetFunctions.size()));

        DecompInterface decomp = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        decomp.setOptions(options);
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(true);
        decomp.setSimplificationStyle("decompile");
        decomp.openProgram(currentProgram);

        File outFile = new File(outJsonPath);
        if (!outFile.isAbsolute()) {
            outFile = new File(System.getProperty("user.dir"), outJsonPath);
        }
        if (outFile.getParentFile() != null) {
            outFile.getParentFile().mkdirs();
        }

        try (BufferedWriter w = new BufferedWriter(
                new OutputStreamWriter(new FileOutputStream(outFile), StandardCharsets.UTF_8), 131072)) {
            w.write("{\n");
            w.write("  \"functions\": [\n");
            for (int i = 0; i < targetFunctions.size(); i++) {
                Function fn = targetFunctions.get(i);
                long entryLong = fn.getEntryPoint().getOffset();
                long fileOff = entryLong - 0x8001E2E0L;
                DecompileResults res = decomp.decompileFunction(fn, 30, monitor);
                String cCode = "";
                String sig = fn.getPrototypeString(false, false);
                if (res != null && res.decompileCompleted() && res.getDecompiledFunction() != null) {
                    cCode = res.getDecompiledFunction().getC();
                    sig = res.getDecompiledFunction().getSignature();
                }

                // Coleta tambem o assembly desmontado da funcao
                StringBuilder asmSb = new StringBuilder();
                Instruction insn = listing.getInstructionAt(fn.getEntryPoint());
                int maxInsns = 250;
                int count = 0;
                while (insn != null && count < maxInsns) {
                    long iAddr = insn.getAddress().getOffset();
                    if (count > 0 && funcStarts.contains(iAddr)) {
                        break;
                    }
                    asmSb.append(String.format("0x%08X: %s\n", iAddr, insn.toString()));
                    count++;
                    if ("blr".equalsIgnoreCase(insn.getMnemonicString())) {
                        break;
                    }
                    insn = insn.getNext();
                }

                w.write("    {\n");
                w.write(String.format("      \"name\": \"%s\",\n", escapeJson(fn.getName())));
                w.write(String.format("      \"address\": \"0x%08X\",\n", entryLong));
                w.write(String.format("      \"file_offset\": \"0x%06X\",\n", fileOff));
                w.write(String.format("      \"signature\": \"%s\",\n", escapeJson(sig)));
                w.write(String.format("      \"decompiled_c\": \"%s\",\n", escapeJson(cCode)));
                w.write(String.format("      \"disassembly\": \"%s\"\n", escapeJson(asmSb.toString())));
                w.write(i + 1 < targetFunctions.size() ? "    },\n" : "    }\n");
            }
            w.write("  ]\n");
            w.write("}\n");
        }

        decomp.dispose();
        println("[ DecompileBattleRoutines ] Exportacao concluida em: " + outFile.getAbsolutePath());
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
