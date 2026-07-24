package org.primer3;

import com.sun.jna.Library;
import com.sun.jna.Native;

public interface Primer3Library extends Library {
    // Automatically load the library named "primer3_thermo"
    // JNA will look for it in the classpath (e.g. linux-x86-64/libprimer3_thermo.so, win32-x86-64/primer3_thermo.dll)
    Primer3Library INSTANCE = Native.load("primer3_thermo", Primer3Library.class);

    /**
     * Configures the folder path containing thermodynamic parameter files.
     */
    void wasm_set_config_path(String path);

    /**
     * Calculate single sequence Tm and deltaG.
     */
    String wasm_oligotm(String seq, 
                        double dna_conc, 
                        double salt_conc, 
                        double divalent_conc, 
                        double dntp_conc, 
                        double dmso_conc, 
                        double dmso_fact, 
                        double formamide_conc, 
                        int nn_max_len,
                        int tm_method, 
                        int salt_corrections, 
                        double annealing_temp);

    /**
     * Calculate Hairpin properties & secondary structures.
     */
    String wasm_hairpin(String seq, 
                         double mv, 
                         double dv, 
                         double dntp, 
                         double dna_conc, 
                         double temp);

    /**
     * Calculate Dimer properties & secondary structures.
     */
    String wasm_dimer(String seq1, 
                       String seq2, 
                       int align_type,
                       double mv, 
                       double dv, 
                       double dntp, 
                       double dna_conc, 
                       double temp);
}
