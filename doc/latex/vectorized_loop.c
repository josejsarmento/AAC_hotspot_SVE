int esize = col - 2 * BLOCK_SIZE_C; 
int words_reg = SVE_VL * QUADW_SVE_VL / WORD_SIZE; 
for ( r = r_start; r < r_start + BLOCK_SIZE_R; r+=4 ) {
    float* temp_actual = &temp[r * col + c_start];
    float* temp_down = &temp[(r+1) * col + c_start];
    float* temp_up = &temp[(r-1) * col + c_start];
    float* temp_left = &temp[r * col + c_start - 1];
    float* temp_right = &temp[r * col + c_start + 1];
    float* power_actual = &power[r * col + c_start];
    float* result_actual = &result[r * col + c_start];
	    
    asm volatile(
    "mov x4, #0\n\t"
    "mov x5, %[col]\n\t"
    "mov x6, %[col2]\n\t"
    "mov x7, %[col3]\n\t"

    "whilelt p0.s, x4, %[words_reg]\n\t"
    "ld1rw z0.s, p0/z, %[Rx_1]\n\t"
    "ld1rw z1.s, p0/z, %[Ry_1]\n\t"
    "ld1rw z2.s, p0/z, %[Rz_1]\n\t"
    "ld1rw z3.s, p0/z, %[amb_temp]\n\t"
    "ld1rw z4.s, p0/z, %[cap_1]\n\t"

    "whilelt p1.s, x4, %[size]\n\t"
    ".loop_column:\n\t"
    "ld1w z5.s,  p0/z, [%[temp], x4, lsl #2]\n\t"
    "ld1w z10.s,  p0/z, [%[temp], x5, lsl #2]\n\t"
    "ld1w z15.s,  p0/z, [%[temp], x6, lsl #2]\n\t"
    "ld1w z20.s,  p0/z, [%[temp], x7, lsl #2]\n\t"
    
    "mov z6.d, z3.d\n\t"
    "mov z11.d, z3.d\n\t"
    "mov z16.d, z3.d\n\t"
    "mov z21.d, z3.d\n\t"

    "fsub z6.s, p0/m, z6.s, z5.s\n\t"
    "fsub z11.s, p0/m, z11.s, z10.s\n\t"
    "fsub z16.s, p0/m, z16.s, z15.s\n\t"
    "fsub z21.s, p0/m, z21.s, z20.s\n\t"

    "fmul z6.s, p0/m, z6.s, z2.s\n\t"
    "fmul z11.s, p0/m, z11.s, z2.s\n\t"
    "fmul z16.s, p0/m, z16.s, z2.s\n\t"
    "fmul z21.s, p0/m, z21.s, z2.s\n\t"

    "ld1w z7.s, p0/z, [%[temp_up], x4, lsl #2]\n\t"
    "ld1w z8.s, p0/z, [%[temp_down], x4, lsl #2]\n\t"
    "ld1w z12.s, p0/z, [%[temp_up], x5, lsl #2]\n\t"
    "ld1w z13.s, p0/z, [%[temp_down], x5, lsl #2]\n\t"
    "ld1w z17.s, p0/z, [%[temp_up], x6, lsl #2]     \n\t"
    "ld1w z18.s, p0/z, [%[temp_down], x6, lsl #2]\n\t"
    "ld1w z22.s, p0/z, [%[temp_up], x7, lsl #2]\n\t"
    "ld1w z23.s, p0/z, [%[temp_down], x7, lsl #2]\n\t"

    "fadd z8.s, p0/m, z8.s, z7.s\n\t"
    "fadd z13.s, p0/m, z13.s, z12.s\n\t"
    "fadd z18.s, p0/m, z18.s, z17.s\n\t"
    "fadd z23.s, p0/m, z23.s, z22.s\n\t"

    "mov z7.d, z5.d\n\t"
    "mov z12.d, z10.d\n\t"
    "mov z17.d, z15.d\n\t"
    "mov z22.d, z20.d\n\t"

    "fadd z7.s, p0/m, z7.s, z7.s\n\t"
    "fadd z12.s, p0/m, z12.s, z12.s\n\t"
    "fadd z17.s, p0/m, z17.s, z17.s\n\t"
    "fadd z22.s, p0/m, z22.s, z22.s\n\t"

    "fsub z8.s, p0/m, z8.s, z7.s\n\t"
    "fsub z13.s, p0/m, z13.s, z12.s\n\t"
    "fsub z18.s, p0/m, z18.s, z17.s\n\t"
    "fsub z23.s, p0/m, z23.s, z22.s\n\t"

    "fmul z8.s, p0/m, z8.s, z2.s\n\t"
    "fmul z13.s, p0/m, z13.s, z2.s\n\t"
    "fmul z18.s, p0/m, z18.s, z12.s\n\t"
    "fmul z23.s, p0/m, z23.s, z2.s\n\t"

    "fadd z6.s, p0/m, z6.s, z8.s\n\t"
    "fadd z11.s, p0/m, z11.s, z13.s\n\t"
    "fadd z16.s, p0/m, z16.s, z18.s\n\t"
    "fadd z21.s, p0/m, z21.s, z23.s\n\t"

    "ld1w z8.s, p0/z, [%[temp_left], x4, lsl #2]    \n\t"
    "ld1w z9.s, p0/z, [%[temp_right], x4, lsl #2]   \n\t"
    "ld1w z13.s, p0/z, [%[temp_left], x5, lsl #2]   \n\t"
    "ld1w z14.s, p0/z, [%[temp_right], x5, lsl #2]  \n\t"
    "ld1w z18.s, p0/z, [%[temp_left], x6, lsl #2]   \n\t"
    "ld1w z19.s, p0/z, [%[temp_right], x6, lsl #2]  \n\t"
    "ld1w z23.s, p0/z, [%[temp_left], x7, lsl #2]   \n\t"
    "ld1w z24.s, p0/z, [%[temp_right], x7, lsl #2]  \n\t"
    
    "fadd z9.s, p0/m, z9.s, z8.s\n\t"
    "fadd z14.s, p0/m, z14.s, z13.s\n\t"
    "fadd z19.s, p0/m, z19.s, z18.s\n\t"
    "fadd z24.s, p0/m, z24.s, z23.s\n\t" 
    
    "fsub z9.s, p0/m, z9.s, z7.s\n\t"
    "fsub z14.s, p0/m, z14.s, z12.s\n\t"
    "fsub z19.s, p0/m, z19.s, z17.s\n\t"
    "fsub z24.s, p0/m, z24.s, z22.s\n\t"

    "fmul z9.s, p0/m, z9.s, z0.s\n\t"
    "fmul z14.s, p0/m, z14.s, z0.s\n\t"
    "fmul z19.s, p0/m, z19.s, z0.s\n\t"
    "fmul z24.s, p0/m, z24.s, z0.s\n\t"

    "fadd z6.s, p0/m, z6.s, z9.s\n\t"
    "fadd z11.s, p0/m, z11.s, z14.s\n\t"
    "fadd z16.s, p0/m, z16.s, z19.s\n\t"
    "fadd z21.s, p0/m, z21.s, z24.s\n\t"

    "ld1w z7.s, p0/z, [%[power], x4, lsl #2]\n\t"
    "ld1w z12.s, p0/z, [%[power], x5, lsl #2]\n\t"
    "ld1w z17.s, p0/z, [%[power], x6, lsl #2]\n\t"
    "ld1w z22.s, p0/z, [%[power], x7, lsl #2]\n\t" 

    "fadd z6.s, p0/m, z6.s, z7.s\n\t"
    "fadd z11.s, p0/m, z11.s, z12.s\n\t"
    "fadd z16.s, p0/m, z16.s, z17.s\n\t"
    "fadd z21.s, p0/m, z21.s, z22.s\n\t"

    "fmul z6.s, p0/m, z6.s, z4.s\n\t"
    "fmul z11.s, p0/m, z11.s, z4.s\n\t"
    "fmul z16.s, p0/m, z16.s, z4.sn\t"
    "fmul z21.s, p0/m, z21.s, z4.s\n\t"

    "fadd z6.s, p0/m, z6.s, z5.s\n\t"
    "fadd z11.s, p0/m, z11.s, z5.s\n\t"
    "fadd z16.s, p0/m, z16.s, z5.s\n\t"
    "fadd z21.s, p0/m, z21.s, z5.s\n\t"

    "st1w z6.s, p0, [%[result], x4, lsl #2]\n\t"
    "st1w z11.s, p0, [%[result], x5, lsl #2]\n\t"
    "st1w z16.s, p0, [%[result], x6, lsl #2]\n\t"
    "st1w z21.s, p0, [%[result], x7, lsl #2]\n\t"
    
    "incw x4\n\t"
    "incw x5\n\t"
    "incw x6\n\t"
    "incw x7\n\t"
    "whilelt p1.s, x4, %[size]\n\t"
    "b.first .loop_column\n\t"
    :   [result]        "+r" (result_actual)
    :   [temp]          "r"  (temp_actual), 
        [temp_down]     "r"  (temp_down), 
        [temp_up]       "r"  (temp_up), 
        [temp_left]     "r"  (temp_left), 
        [temp_right]    "r"  (temp_right), 
        [power]         "r"  (power_actual),
        [size]          "r"  (esize),
        [words_reg]     "r"  (words_reg) 
        , 
        [Rx_1]      "m" (Rx_1), 
        [Ry_1]      "m" (Ry_1), 
        [Rz_1]      "m" (Rz_1),
        [amb_temp]  "m" (amb_temp), 
        [cap_1]     "m" (Cap_1),
        [col] "r" (col), 
        [col2] "r" (col*2), 
        [col3] "r" (col*3)
    :   "x4", 
        "memory", 
        "p0", 
        "z0", "z1", "z2", "z3", "z4", 
        "z5", "z6", "z7", "z8", "z9"
        ,
        "x5",
        "z10", "z11", "z12", "z13", "z14",
        "x6",
        "z15", "z16", "z17", "z18", "z19",
        "x7",
        "z20", "z21", "z22", "z23", "z24"
    );
}
chunk += (col / BLOCK_SIZE_C) - 3;
