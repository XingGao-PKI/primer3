package org.primer3;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.util.Map;

/**
 * A highly readable, idiomatic Java wrapper around Primer3's native thermodynamic engine.
 * Under the hood, it leverages JNA for native calls and Jackson for parsing JSON results.
 */
public class Primer3Thermo {
    private static final ObjectMapper mapper = new ObjectMapper();
    private static java.nio.file.Path tempConfigDir = null;

    static {
        try {
            tempConfigDir = java.nio.file.Files.createTempDirectory("primer3_config");
            tempConfigDir.toFile().deleteOnExit();
            String[] configFiles = {
                "dangle.dh", "dangle.ds", "loops.dh", "loops.ds",
                "stack.dh", "stack.ds", "stackmm.dh", "stackmm.ds",
                "tetraloop.dh", "tetraloop.ds", "triloop.dh", "triloop.ds",
                "tstack.dh", "tstack_tm_inf.ds", "tstack2.dh", "tstack2.ds"
            };
            for (String file : configFiles) {
                java.nio.file.Path targetPath = tempConfigDir.resolve(file);
                targetPath.toFile().deleteOnExit();
                try (java.io.InputStream is = Primer3Thermo.class.getResourceAsStream("/primer3_config/" + file)) {
                    if (is != null) {
                        java.nio.file.Files.copy(is, targetPath, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
                    }
                }
            }
            Primer3Library.INSTANCE.wasm_set_config_path(tempConfigDir.toAbsolutePath().toString() + "/");
        } catch (Exception e) {
            System.err.println("Warning: Failed to extract embedded primer3_config: " + e.getMessage());
        }
    }

    public static class TmResult {
        public double Tm;
        public double bound;
        public double dg;
        public boolean symmetry;

        @Override
        public String toString() {
            return "TmResult{Tm=" + Tm + " °C, bound=" + bound + ", dg=" + dg + " cal/mol, symmetry=" + symmetry + "}";
        }
    }

    public static class StructureResult {
        public double temp;
        public double dg;
        public double dh;
        public double ds;
        public int align_end_1;
        public int align_end_2;
        public String structure;

        @Override
        public String toString() {
            return "StructureResult{temp=" + temp + " °C, dg=" + dg + " cal/mol, structure=\n" + structure + "}";
        }
    }

    /**
     * Calculates melting temperature (Tm) and thermodynamic properties for a single DNA sequence.
     */
    public static TmResult calculateTm(String sequence) throws IOException {
        // Defaults matching standard Primer3 parameters
        return calculateTm(sequence, 50.0, 50.0, 1.5, 0.6, 0.0, 0.6, 0.0, 60, 1, 1, 50.0);
    }

    public static TmResult calculateTm(String sequence, 
                                       double dnaConc, 
                                       double saltConc, 
                                       double divalentConc, 
                                       double dntpConc, 
                                       double dmsoConc, 
                                       double dmsoFact, 
                                       double formamideConc, 
                                       int nnMaxLen,
                                       int tmMethod, 
                                       int saltCorrections, 
                                       double annealingTemp) throws IOException {
        String jsonStr = Primer3Library.INSTANCE.wasm_oligotm(
            sequence, dnaConc, saltConc, divalentConc, dntpConc, 
            dmsoConc, dmsoFact, formamideConc, nnMaxLen, 
            tmMethod, saltCorrections, annealingTemp
        );
        checkError(jsonStr);
        return mapper.readValue(jsonStr, TmResult.class);
    }

    /**
     * Analyzes hairpin secondary structure for a sequence.
     */
    public static StructureResult analyzeHairpin(String sequence) throws IOException {
        return analyzeHairpin(sequence, 50.0, 0.0, 0.8, 50.0, 37.0);
    }

    public static StructureResult analyzeHairpin(String sequence, 
                                                 double mv, 
                                                 double dv, 
                                                 double dntp, 
                                                 double dnaConc, 
                                                 double temp) throws IOException {
        String jsonStr = Primer3Library.INSTANCE.wasm_hairpin(sequence, mv, dv, dntp, dnaConc, temp);
        checkError(jsonStr);
        return mapper.readValue(jsonStr, StructureResult.class);
    }

    /**
     * Analyzes dimer structure between two sequences.
     * alignType - 1: ANY, 2: END1, 3: END2
     */
    public static StructureResult analyzeDimer(String sequence1, String sequence2, int alignType) throws IOException {
        return analyzeDimer(sequence1, sequence2, alignType, 50.0, 0.0, 0.8, 50.0, 37.0);
    }

    public static StructureResult analyzeDimer(String sequence1, 
                                               String sequence2, 
                                               int alignType,
                                               double mv, 
                                               double dv, 
                                               double dntp, 
                                               double dnaConc, 
                                               double temp) throws IOException {
        String jsonStr = Primer3Library.INSTANCE.wasm_dimer(sequence1, sequence2, alignType, mv, dv, dntp, dnaConc, temp);
        checkError(jsonStr);
        return mapper.readValue(jsonStr, StructureResult.class);
    }

    private static void checkError(String jsonStr) throws IOException {
        if (jsonStr == null || jsonStr.isEmpty()) {
            throw new IOException("Empty response from native Primer3 library.");
        }
        if (jsonStr.contains("\"error\":")) {
            Map<?, ?> errMap = mapper.readValue(jsonStr, Map.class);
            throw new IOException("Native Error: " + errMap.get("error"));
        }
    }
}
