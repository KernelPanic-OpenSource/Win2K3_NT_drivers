//
// Color Look Up Table (cLUTs) used to convert UYVY to RGB8
//
unsigned char const PalTable[] = {
  0,   0,   0,  PC_NOCOLLAPSE,     //    0
128,   0,   0,  PC_NOCOLLAPSE,     //    1
  0, 128,   0,  PC_NOCOLLAPSE,     //    2
128, 128,   0,  PC_NOCOLLAPSE,     //    3
  0,   0, 128,  PC_NOCOLLAPSE,     //    4
128,   0, 128,  PC_NOCOLLAPSE,     //    5
  0, 128, 128,  PC_NOCOLLAPSE,     //    6
192, 192, 192,  PC_NOCOLLAPSE,     //    7
192, 220, 192,  PC_NOCOLLAPSE,     //    8
166, 202, 240,  PC_NOCOLLAPSE,     //    9

  0,   0,   0,  PC_NOCOLLAPSE,     //   10
  0,   0,   0,  PC_NOCOLLAPSE,     //   11
  0,   0,   0,  PC_NOCOLLAPSE,     //   12
  0,   0,   0,  PC_NOCOLLAPSE,     //   13
  0,   0,   0,  PC_NOCOLLAPSE,     //   14
  0,   0,   0,  PC_NOCOLLAPSE,     //   15
  0,  67,   0,  PC_NOCOLLAPSE,     //   16
  0,  41,   0,  PC_NOCOLLAPSE,     //   17
 35,  15,   0,  PC_NOCOLLAPSE,     //   18
 86,   0,   0,  PC_NOCOLLAPSE,     //   19
  0,  55,   0,  PC_NOCOLLAPSE,     //   20
  0,  29,   0,  PC_NOCOLLAPSE,     //   21
 35,   3,   0,  PC_NOCOLLAPSE,     //   22
 86,   0,   0,  PC_NOCOLLAPSE,     //   23
  0,  42,  42,  PC_NOCOLLAPSE,     //   24
  0,  16,  42,  PC_NOCOLLAPSE,     //   25
 35,   0,  42,  PC_NOCOLLAPSE,     //   26
 86,   0,  42,  PC_NOCOLLAPSE,     //   27
  0,  30, 106,  PC_NOCOLLAPSE,     //   28
  0,   4, 106,  PC_NOCOLLAPSE,     //   29
 35,   0, 106,  PC_NOCOLLAPSE,     //   30
 86,   0, 106,  PC_NOCOLLAPSE,     //   31
  0,  86,   0,  PC_NOCOLLAPSE,     //   32
  2,  60,   0,  PC_NOCOLLAPSE,     //   33
 53,  34,   0,  PC_NOCOLLAPSE,     //   34
105,   8,   0,  PC_NOCOLLAPSE,     //   35
  0,  73,   0,  PC_NOCOLLAPSE,     //   36
  2,  47,   0,  PC_NOCOLLAPSE,     //   37
 53,  21,   0,  PC_NOCOLLAPSE,     //   38
105,   0,   0,  PC_NOCOLLAPSE,     //   39
  0,  61,  60,  PC_NOCOLLAPSE,     //   40
  2,  35,  60,  PC_NOCOLLAPSE,     //   41
 53,   9,  60,  PC_NOCOLLAPSE,     //   42
105,   0,  60,  PC_NOCOLLAPSE,     //   43
  0,  48, 125,  PC_NOCOLLAPSE,     //   44
  2,  22, 125,  PC_NOCOLLAPSE,     //   45
 53,   0, 125,  PC_NOCOLLAPSE,     //   46
105,   0, 125,  PC_NOCOLLAPSE,     //   47
  0, 104,   0,  PC_NOCOLLAPSE,     //   48
 21,  78,   0,  PC_NOCOLLAPSE,     //   49
 72,  52,   0,  PC_NOCOLLAPSE,     //   50
123,  26,   0,  PC_NOCOLLAPSE,     //   51
  0,  92,  14,  PC_NOCOLLAPSE,     //   52
 21,  66,  14,  PC_NOCOLLAPSE,     //   53
 72,  40,  14,  PC_NOCOLLAPSE,     //   54
123,  14,  14,  PC_NOCOLLAPSE,     //   55
  0,  79,  79,  PC_NOCOLLAPSE,     //   56
 21,  53,  79,  PC_NOCOLLAPSE,     //   57
 72,  27,  79,  PC_NOCOLLAPSE,     //   58
123,   1,  79,  PC_NOCOLLAPSE,     //   59
  0,  67, 143,  PC_NOCOLLAPSE,     //   60
 21,  41, 143,  PC_NOCOLLAPSE,     //   61
 72,  15, 143,  PC_NOCOLLAPSE,     //   62
123,   0, 143,  PC_NOCOLLAPSE,     //   63
  0, 123,   0,  PC_NOCOLLAPSE,     //   64
 40,  97,   0,  PC_NOCOLLAPSE,     //   65
 91,  71,   0,  PC_NOCOLLAPSE,     //   66
142,  45,   0,  PC_NOCOLLAPSE,     //   67
  0, 110,  33,  PC_NOCOLLAPSE,     //   68
 40,  84,  33,  PC_NOCOLLAPSE,     //   69
 91,  58,  33,  PC_NOCOLLAPSE,     //   70
142,  32,  33,  PC_NOCOLLAPSE,     //   71
  0,  98,  97,  PC_NOCOLLAPSE,     //   72
 40,  72,  97,  PC_NOCOLLAPSE,     //   73
 91,  46,  97,  PC_NOCOLLAPSE,     //   74
142,  20,  97,  PC_NOCOLLAPSE,     //   75
  0,  85, 162,  PC_NOCOLLAPSE,     //   76
 40,  59, 162,  PC_NOCOLLAPSE,     //   77
 91,  33, 162,  PC_NOCOLLAPSE,     //   78
142,   7, 162,  PC_NOCOLLAPSE,     //   79
  7, 142,   0,  PC_NOCOLLAPSE,     //   80
 58, 116,   0,  PC_NOCOLLAPSE,     //   81
109,  90,   0,  PC_NOCOLLAPSE,     //   82
160,  64,   0,  PC_NOCOLLAPSE,     //   83
  7, 129,  52,  PC_NOCOLLAPSE,     //   84
 58, 103,  52,  PC_NOCOLLAPSE,     //   85
109,  77,  52,  PC_NOCOLLAPSE,     //   86
160,  51,  52,  PC_NOCOLLAPSE,     //   87
  7, 117, 116,  PC_NOCOLLAPSE,     //   88
 58,  91, 116,  PC_NOCOLLAPSE,     //   89
109,  65, 116,  PC_NOCOLLAPSE,     //   90
160,  39, 116,  PC_NOCOLLAPSE,     //   91
  7, 104, 181,  PC_NOCOLLAPSE,     //   92
 58,  78, 181,  PC_NOCOLLAPSE,     //   93
109,  52, 181,  PC_NOCOLLAPSE,     //   94
160,  26, 181,  PC_NOCOLLAPSE,     //   95
 26, 160,   6,  PC_NOCOLLAPSE,     //   96
 77, 134,   6,  PC_NOCOLLAPSE,     //   97
128, 108,   6,  PC_NOCOLLAPSE,     //   98
179,  82,   6,  PC_NOCOLLAPSE,     //   99
 26, 148,  70,  PC_NOCOLLAPSE,     //  100
 77, 122,  70,  PC_NOCOLLAPSE,     //  101
128,  96,  70,  PC_NOCOLLAPSE,     //  102
179,  70,  70,  PC_NOCOLLAPSE,     //  103
 26, 135, 135,  PC_NOCOLLAPSE,     //  104
 77, 109, 135,  PC_NOCOLLAPSE,     //  105
128,  83, 135,  PC_NOCOLLAPSE,     //  106
179,  57, 135,  PC_NOCOLLAPSE,     //  107
 26, 123, 199,  PC_NOCOLLAPSE,     //  108
 77,  97, 199,  PC_NOCOLLAPSE,     //  109
128,  71, 199,  PC_NOCOLLAPSE,     //  110
179,  45, 199,  PC_NOCOLLAPSE,     //  111
 44, 179,  24,  PC_NOCOLLAPSE,     //  112
 96, 153,  24,  PC_NOCOLLAPSE,     //  113
147, 127,  24,  PC_NOCOLLAPSE,     //  114
198, 101,  24,  PC_NOCOLLAPSE,     //  115
 44, 166,  89,  PC_NOCOLLAPSE,     //  116
 96, 140,  89,  PC_NOCOLLAPSE,     //  117
147, 114,  89,  PC_NOCOLLAPSE,     //  118
198,  88,  89,  PC_NOCOLLAPSE,     //  119
 44, 154, 153,  PC_NOCOLLAPSE,     //  120
 96, 128, 153,  PC_NOCOLLAPSE,     //  121
147, 102, 153,  PC_NOCOLLAPSE,     //  122
198,  76, 153,  PC_NOCOLLAPSE,     //  123
 44, 141, 218,  PC_NOCOLLAPSE,     //  124
 96, 115, 218,  PC_NOCOLLAPSE,     //  125
147,  89, 218,  PC_NOCOLLAPSE,     //  126
198,  63, 218,  PC_NOCOLLAPSE,     //  127
 63, 198,  43,  PC_NOCOLLAPSE,     //  128
114, 172,  43,  PC_NOCOLLAPSE,     //  129
165, 146,  43,  PC_NOCOLLAPSE,     //  130
216, 120,  43,  PC_NOCOLLAPSE,     //  131
 63, 185, 107,  PC_NOCOLLAPSE,     //  132
114, 159, 107,  PC_NOCOLLAPSE,     //  133
165, 133, 107,  PC_NOCOLLAPSE,     //  134
216, 107, 107,  PC_NOCOLLAPSE,     //  135
 63, 172, 172,  PC_NOCOLLAPSE,     //  136
114, 146, 172,  PC_NOCOLLAPSE,     //  137
165, 120, 172,  PC_NOCOLLAPSE,     //  138
216,  94, 172,  PC_NOCOLLAPSE,     //  139
 63, 160, 237,  PC_NOCOLLAPSE,     //  140
114, 134, 237,  PC_NOCOLLAPSE,     //  141
165, 108, 237,  PC_NOCOLLAPSE,     //  142
216,  82, 237,  PC_NOCOLLAPSE,     //  143
 82, 216,  62,  PC_NOCOLLAPSE,     //  144
133, 190,  62,  PC_NOCOLLAPSE,     //  145
184, 164,  62,  PC_NOCOLLAPSE,     //  146
235, 138,  62,  PC_NOCOLLAPSE,     //  147
 82, 204, 126,  PC_NOCOLLAPSE,     //  148
133, 178, 126,  PC_NOCOLLAPSE,     //  149
184, 152, 126,  PC_NOCOLLAPSE,     //  150
235, 126, 126,  PC_NOCOLLAPSE,     //  151
 82, 191, 191,  PC_NOCOLLAPSE,     //  152
133, 165, 191,  PC_NOCOLLAPSE,     //  153
184, 139, 191,  PC_NOCOLLAPSE,     //  154
235, 113, 191,  PC_NOCOLLAPSE,     //  155
 82, 179, 255,  PC_NOCOLLAPSE,     //  156
133, 153, 255,  PC_NOCOLLAPSE,     //  157
184, 127, 255,  PC_NOCOLLAPSE,     //  158
235, 101, 255,  PC_NOCOLLAPSE,     //  159
100, 235,  80,  PC_NOCOLLAPSE,     //  160
151, 209,  80,  PC_NOCOLLAPSE,     //  161
203, 183,  80,  PC_NOCOLLAPSE,     //  162
254, 157,  80,  PC_NOCOLLAPSE,     //  163
100, 222, 145,  PC_NOCOLLAPSE,     //  164
151, 196, 145,  PC_NOCOLLAPSE,     //  165
203, 170, 145,  PC_NOCOLLAPSE,     //  166
254, 144, 145,  PC_NOCOLLAPSE,     //  167
100, 210, 209,  PC_NOCOLLAPSE,     //  168
151, 184, 209,  PC_NOCOLLAPSE,     //  169
203, 158, 209,  PC_NOCOLLAPSE,     //  170
254, 132, 209,  PC_NOCOLLAPSE,     //  171
100, 197, 255,  PC_NOCOLLAPSE,     //  172
151, 171, 255,  PC_NOCOLLAPSE,     //  173
203, 145, 255,  PC_NOCOLLAPSE,     //  174
254, 119, 255,  PC_NOCOLLAPSE,     //  175
119, 253,  99,  PC_NOCOLLAPSE,     //  176
170, 227,  99,  PC_NOCOLLAPSE,     //  177
221, 201,  99,  PC_NOCOLLAPSE,     //  178
255, 175,  99,  PC_NOCOLLAPSE,     //  179
119, 241, 163,  PC_NOCOLLAPSE,     //  180
170, 215, 163,  PC_NOCOLLAPSE,     //  181
221, 189, 163,  PC_NOCOLLAPSE,     //  182
255, 163, 163,  PC_NOCOLLAPSE,     //  183
119, 228, 228,  PC_NOCOLLAPSE,     //  184
170, 202, 228,  PC_NOCOLLAPSE,     //  185
221, 176, 228,  PC_NOCOLLAPSE,     //  186
255, 150, 228,  PC_NOCOLLAPSE,     //  187
119, 216, 255,  PC_NOCOLLAPSE,     //  188
170, 190, 255,  PC_NOCOLLAPSE,     //  189
221, 164, 255,  PC_NOCOLLAPSE,     //  190
255, 138, 255,  PC_NOCOLLAPSE,     //  191
138, 255, 117,  PC_NOCOLLAPSE,     //  192
189, 246, 117,  PC_NOCOLLAPSE,     //  193
240, 220, 117,  PC_NOCOLLAPSE,     //  194
255, 194, 117,  PC_NOCOLLAPSE,     //  195
138, 255, 182,  PC_NOCOLLAPSE,     //  196
189, 234, 182,  PC_NOCOLLAPSE,     //  197
240, 208, 182,  PC_NOCOLLAPSE,     //  198
255, 181, 182,  PC_NOCOLLAPSE,     //  199
138, 247, 247,  PC_NOCOLLAPSE,     //  200
189, 221, 247,  PC_NOCOLLAPSE,     //  201
240, 195, 247,  PC_NOCOLLAPSE,     //  202
255, 169, 247,  PC_NOCOLLAPSE,     //  203
138, 234, 255,  PC_NOCOLLAPSE,     //  204
189, 208, 255,  PC_NOCOLLAPSE,     //  205
240, 182, 255,  PC_NOCOLLAPSE,     //  206
255, 156, 255,  PC_NOCOLLAPSE,     //  207
156, 255, 136,  PC_NOCOLLAPSE,     //  208
207, 255, 136,  PC_NOCOLLAPSE,     //  209
255, 239, 136,  PC_NOCOLLAPSE,     //  210
255, 213, 136,  PC_NOCOLLAPSE,     //  211
156, 255, 201,  PC_NOCOLLAPSE,     //  212
207, 252, 201,  PC_NOCOLLAPSE,     //  213
255, 226, 201,  PC_NOCOLLAPSE,     //  214
255, 200, 201,  PC_NOCOLLAPSE,     //  215
156, 255, 255,  PC_NOCOLLAPSE,     //  216
207, 240, 255,  PC_NOCOLLAPSE,     //  217
255, 214, 255,  PC_NOCOLLAPSE,     //  218
255, 188, 255,  PC_NOCOLLAPSE,     //  219
156, 253, 255,  PC_NOCOLLAPSE,     //  220
207, 227, 255,  PC_NOCOLLAPSE,     //  221
255, 201, 255,  PC_NOCOLLAPSE,     //  222
255, 175, 255,  PC_NOCOLLAPSE,     //  223
175, 255, 155,  PC_NOCOLLAPSE,     //  224
226, 255, 155,  PC_NOCOLLAPSE,     //  225
255, 255, 155,  PC_NOCOLLAPSE,     //  226
255, 231, 155,  PC_NOCOLLAPSE,     //  227
175, 255, 219,  PC_NOCOLLAPSE,     //  228
226, 255, 219,  PC_NOCOLLAPSE,     //  229
255, 245, 219,  PC_NOCOLLAPSE,     //  230
255, 219, 219,  PC_NOCOLLAPSE,     //  231
175, 255, 255,  PC_NOCOLLAPSE,     //  232
226, 255, 255,  PC_NOCOLLAPSE,     //  233
255, 232, 255,  PC_NOCOLLAPSE,     //  234
255, 206, 255,  PC_NOCOLLAPSE,     //  235
175, 255, 255,  PC_NOCOLLAPSE,     //  236
226, 246, 255,  PC_NOCOLLAPSE,     //  237
255, 220, 255,  PC_NOCOLLAPSE,     //  238
255, 194, 255,  PC_NOCOLLAPSE,     //  239
  0,   0,   0,  PC_NOCOLLAPSE,     //  240
  0,   0,   0,  PC_NOCOLLAPSE,     //  241
  0,   0,   0,  PC_NOCOLLAPSE,     //  242
  0,   0,   0,  PC_NOCOLLAPSE,     //  243
  0,   0,   0,  PC_NOCOLLAPSE,     //  244
  0,   0,   0,  PC_NOCOLLAPSE,     //  245

255, 251, 240,  PC_NOCOLLAPSE,     //  246
160, 160, 164,  PC_NOCOLLAPSE,     //  247
128, 128, 128,  PC_NOCOLLAPSE,     //  248
255,   0,   0,  PC_NOCOLLAPSE,     //  249
  0, 255,   0,  PC_NOCOLLAPSE,     //  250
255, 255,   0,  PC_NOCOLLAPSE,     //  251
  0,   0, 255,  PC_NOCOLLAPSE,     //  252
255,   0, 255,  PC_NOCOLLAPSE,     //  253
  0, 255, 255,  PC_NOCOLLAPSE,     //  254
255, 255, 255,  PC_NOCOLLAPSE      //  255
};

unsigned long yLUT_0[272] = {
    0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, //  00
    0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, //  01
    0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, //  02
    0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, //  03
    0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, 0x00000010, //  04
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x00000020, //  05
    0x00000020, 0x00000020, 0x00000020, 0x00000020, 0x00000030, 0x00000030, 0x00000030, 0x00000030, //  06
    0x00000030, 0x00000030, 0x00000030, 0x00000030, 0x00000030, 0x00000030, 0x00000030, 0x00000030, //  07
    0x00000030, 0x00000030, 0x00000030, 0x00000030, 0x00000040, 0x00000040, 0x00000040, 0x00000040, //  08
    0x00000040, 0x00000040, 0x00000040, 0x00000040, 0x00000040, 0x00000040, 0x00000040, 0x00000040, //  10
    0x00000040, 0x00000040, 0x00000040, 0x00000040, 0x00000050, 0x00000050, 0x00000050, 0x00000050, //  11
    0x00000050, 0x00000050, 0x00000050, 0x00000050, 0x00000050, 0x00000050, 0x00000050, 0x00000050, //  12
    0x00000050, 0x00000050, 0x00000050, 0x00000050, 0x00000060, 0x00000060, 0x00000060, 0x00000060, //  13
    0x00000060, 0x00000060, 0x00000060, 0x00000060, 0x00000060, 0x00000060, 0x00000060, 0x00000060, //  14
    0x00000060, 0x00000060, 0x00000060, 0x00000060, 0x00000070, 0x00000070, 0x00000070, 0x00000070, //  15
    0x00000070, 0x00000070, 0x00000070, 0x00000070, 0x00000070, 0x00000070, 0x00000070, 0x00000070, //  16
    0x00000070, 0x00000070, 0x00000070, 0x00000070, 0x00000080, 0x00000080, 0x00000080, 0x00000080, //  17
    0x00000080, 0x00000080, 0x00000080, 0x00000080, 0x00000080, 0x00000080, 0x00000080, 0x00000080, //  18
    0x00000080, 0x00000080, 0x00000080, 0x00000080, 0x00000090, 0x00000090, 0x00000090, 0x00000090, //  19
    0x00000090, 0x00000090, 0x00000090, 0x00000090, 0x00000090, 0x00000090, 0x00000090, 0x00000090, //  20
    0x00000090, 0x00000090, 0x00000090, 0x00000090, 0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, //  21
    0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, //  22
    0x000000A0, 0x000000A0, 0x000000A0, 0x000000A0, 0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, //  23
    0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, //  24
    0x000000B0, 0x000000B0, 0x000000B0, 0x000000B0, 0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, //  25
    0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, //  26
    0x000000C0, 0x000000C0, 0x000000C0, 0x000000C0, 0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, //  27
    0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, //  28
    0x000000D0, 0x000000D0, 0x000000D0, 0x000000D0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, //  29
    0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, //  30
    0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, //  31
    0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, //  32
    0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, //  33
    0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0, 0x000000E0  //  34
};

unsigned long yLUT_1[272] = {
    0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, //  00
    0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, //  01
    0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, //  02
    0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, //  03
    0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, //  04
    0x00002000, 0x00002000, 0x00002000, 0x00002000, 0x00002000, 0x00002000, 0x00002000, 0x00002000, //  05
    0x00002000, 0x00002000, 0x00002000, 0x00002000, 0x00003000, 0x00003000, 0x00003000, 0x00003000, //  06
    0x00003000, 0x00003000, 0x00003000, 0x00003000, 0x00003000, 0x00003000, 0x00003000, 0x00003000, //  07
    0x00003000, 0x00003000, 0x00003000, 0x00003000, 0x00004000, 0x00004000, 0x00004000, 0x00004000, //  08
    0x00004000, 0x00004000, 0x00004000, 0x00004000, 0x00004000, 0x00004000, 0x00004000, 0x00004000, //  10
    0x00004000, 0x00004000, 0x00004000, 0x00004000, 0x00005000, 0x00005000, 0x00005000, 0x00005000, //  11
    0x00005000, 0x00005000, 0x00005000, 0x00005000, 0x00005000, 0x00005000, 0x00005000, 0x00005000, //  12
    0x00005000, 0x00005000, 0x00005000, 0x00005000, 0x00006000, 0x00006000, 0x00006000, 0x00006000, //  13
    0x00006000, 0x00006000, 0x00006000, 0x00006000, 0x00006000, 0x00006000, 0x00006000, 0x00006000, //  14
    0x00006000, 0x00006000, 0x00006000, 0x00006000, 0x00007000, 0x00007000, 0x00007000, 0x00007000, //  15
    0x00007000, 0x00007000, 0x00007000, 0x00007000, 0x00007000, 0x00007000, 0x00007000, 0x00007000, //  16
    0x00007000, 0x00007000, 0x00007000, 0x00007000, 0x00008000, 0x00008000, 0x00008000, 0x00008000, //  17
    0x00008000, 0x00008000, 0x00008000, 0x00008000, 0x00008000, 0x00008000, 0x00008000, 0x00008000, //  18
    0x00008000, 0x00008000, 0x00008000, 0x00008000, 0x00009000, 0x00009000, 0x00009000, 0x00009000, //  19
    0x00009000, 0x00009000, 0x00009000, 0x00009000, 0x00009000, 0x00009000, 0x00009000, 0x00009000, //  20
    0x00009000, 0x00009000, 0x00009000, 0x00009000, 0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, //  21
    0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, //  22
    0x0000A000, 0x0000A000, 0x0000A000, 0x0000A000, 0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, //  23
    0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, //  24
    0x0000B000, 0x0000B000, 0x0000B000, 0x0000B000, 0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, //  25
    0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, //  26
    0x0000C000, 0x0000C000, 0x0000C000, 0x0000C000, 0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, //  27
    0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, //  28
    0x0000D000, 0x0000D000, 0x0000D000, 0x0000D000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, //  29
    0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, //  30
    0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, //  31
    0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, //  32
    0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, //  33
    0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000, 0x0000E000  //  34
};



unsigned long cLUT_R0[260] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00010000, 0x00010000, 0x00010000, 0x00010000, 0x00010000, 0x00010000, 0x00010000, 0x00010000,
    0x00010100, 0x00010100, 0x00010100, 0x00010100, 0x00010100, 0x00010100, 0x00010100, 0x00010100,
    0x01010100, 0x01010100, 0x01010100, 0x01010100, 0x01010100, 0x01010100, 0x01010100, 0x01010100,
    0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101, 0x01010101,
    0x01020101, 0x01020101, 0x01020101, 0x01020101, 0x01020101, 0x01020101, 0x01020101, 0x01020101,
    0x01020201, 0x01020201, 0x01020201, 0x01020201, 0x01020201, 0x01020201, 0x01020201, 0x01020201,
    0x02020201, 0x02020201, 0x02020201, 0x02020201, 0x02020201, 0x02020201, 0x02020201, 0x02020201,
    0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202,
    0x02030202, 0x02030202, 0x02030202, 0x02030202, 0x02030202, 0x02030202, 0x02030202, 0x02030202,
    0x02030302, 0x02030302, 0x02030302, 0x02030302, 0x02030302, 0x02030302, 0x02030302, 0x02030302,
    0x03030302, 0x03030302, 0x03030302, 0x03030302, 0x03030302, 0x03030302, 0x03030302, 0x03030302,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303, 0x03030303,
    0x03030303, 0x03030303, 0x03030303, 0x03030303
};


unsigned long cLUT_B0[260] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00040000, 0x00040000, 0x00040000, 0x00040000, 0x00040000, 0x00040000, 0x00040000, 0x00040000,
    0x00040400, 0x00040400, 0x00040400, 0x00040400, 0x00040400, 0x00040400, 0x00040400, 0x00040400,
    0x04040400, 0x04040400, 0x04040400, 0x04040400, 0x04040400, 0x04040400, 0x04040400, 0x04040400,
    0x04040404, 0x04040404, 0x04040404, 0x04040404, 0x04040404, 0x04040404, 0x04040404, 0x04040404,
    0x04080404, 0x04080404, 0x04080404, 0x04080404, 0x04080404, 0x04080404, 0x04080404, 0x04080404,
    0x04080804, 0x04080804, 0x04080804, 0x04080804, 0x04080804, 0x04080804, 0x04080804, 0x04080804,
    0x08080804, 0x08080804, 0x08080804, 0x08080804, 0x08080804, 0x08080804, 0x08080804, 0x08080804,
    0x08080808, 0x08080808, 0x08080808, 0x08080808, 0x08080808, 0x08080808, 0x08080808, 0x08080808,
    0x080C0808, 0x080C0808, 0x080C0808, 0x080C0808, 0x080C0808, 0x080C0808, 0x080C0808, 0x080C0808,
    0x080C0C08, 0x080C0C08, 0x080C0C08, 0x080C0C08, 0x080C0C08, 0x080C0C08, 0x080C0C08, 0x080C0C08,
    0x0C0C0C08, 0x0C0C0C08, 0x0C0C0C08, 0x0C0C0C08, 0x0C0C0C08, 0x0C0C0C08, 0x0C0C0C08, 0x0C0C0C08,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C,
    0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C, 0x0C0C0C0C
};
