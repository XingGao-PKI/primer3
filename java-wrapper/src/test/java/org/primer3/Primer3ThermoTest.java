package org.primer3;

import org.junit.Test;
import static org.junit.Assert.*;

public class Primer3ThermoTest {

    @Test
    public void testTmCalculation() {
        try {
            // This test is designed to run when the native library is present in the classpath.
            // When building the multi-platform jar, this will execute successfully.
            Primer3Thermo.TmResult result = Primer3Thermo.calculateTm("ATCGATCGATCGATCG");
            System.out.println("Result Tm: " + result.Tm);
            assertTrue(result.Tm > 0);
            assertFalse(result.symmetry);
        } catch (UnsatisfiedLinkError e) {
            System.out.println("Skipping test: Native library not loaded (expected during isolated compilation)");
        } catch (Exception e) {
            e.printStackTrace();
            fail("Exception occurred during Tm calculation");
        }
    }
}
