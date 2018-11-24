/*
 * Copyright (c) 2014, Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 */

package com.qualcomm.qti.smartsearch;

/**
 * <p>
 * Implements the Arabic-characters to digits map.
 * </p>
 */
public class DigitMapTableArabic extends DigitMapTableLatin {

    private static DigitMapTableArabic sInstance = null;

    private static final char[][] EXTEND_ARABIC_TABLE = {
            // number 0
            {
                    0x0660
            },
            // number 1
            {
                    0x0661
            },
            // number 2
            {
                    0x0662, 0x0628, 0x0629, 0x062A, 0x062B, 0x066E
            },
            // number 3
            {
                    0x0663, 0x0621, 0x0622, 0x0623, 0x0625, 0x0627
            },
            // number 4
            {
                    0x0664, 0x0633, 0x0634, 0x0635, 0x0636
            },
            // number 5
            {
                    0x0665, 0x062F, 0x0630, 0x0631, 0x0632
            },
            // number 6
            {
                    0x0666, 0x062C, 0x062D, 0x062E
            },
            // number 7
            {
                    0x0667, 0x0624, 0x0626, 0x0646, 0x0647, 0x0648,
                    0x0649, 0x064A
            },
            // number 8
            {
                    0x0668, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645,
                    0x066F
            },
            // number 9
            {
                    0x0669, 0x0637, 0x0638, 0x0639, 0x063A
            }
    };

    public static DigitMapTableArabic getInstance() {
        if (sInstance == null) {
            sInstance = new DigitMapTableArabic();
        }
        return sInstance;
    }

    private DigitMapTableArabic() {
    }

    @Override
    public String toDigits(char c) {
        int idx = -1;
        for (int i = 0; i < KEY_NUM; i++) {
            if (contains(EXTEND_ARABIC_TABLE[i], c)) {
                idx = i;
                break;
            }
        }

        if (idx == -1) {
            return super.toDigits(c);
        } else {
            return Integer.toString(idx);
        }
    }

}
