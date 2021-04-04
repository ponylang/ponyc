primitive IsPrime[A: (Integer[A] val & Unsigned)]
  """Primality test using 6k+-1 optimization."""

  // Using a match table like below provides information to LLVM
  // to allocate static globals rather than stack or heap allocation.
  fun _low_prime_table(index: USize): A val =>
    match index
    | 0 => 2 | 1 => 3 | 2 => 5 | 3 => 7 | 4 => 11 
    | 5 => 13 | 6 => 17 | 7 => 19 | 8 => 23 | 9 => 29 
    | 10 => 31 | 11 => 37 | 12 => 41 | 13 => 43 | 14 => 47 
    | 15 => 53 | 16 => 59 | 17 => 61 | 18 => 67 | 19 => 71 
    | 20 => 73 | 21 => 79 | 22 => 83 | 23 => 89 | 24 => 97
    | 25 => 101 | 26 => 103 | 27 => 107 | 28 => 109 | 29 => 113
    | 30 => 127 | 31 => 131 | 32 => 137 | 33 => 139 | 34 => 149
    | 35 => 151 | 36 => 157 | 37 => 163 | 38 => 167 | 39 => 173
    | 40 => 179 | 41 => 181 | 42 => 191 | 43 => 193 | 44 => 197
    | 45 => 199 | 46 => 211 | 47 => 223 | 48 => 227 | 49 => 229
    | 50 => 233 | 51 => 239 | 52 => 241 | 53 => 251 | 54 => 257
    | 55 => 263 | 56 => 269 | 57 => 271 | 58 => 277 | 59 => 281
    | 60 => 283 | 61 => 293 | 62 => 307 | 63 => 311 | 64 => 313
    | 65 => 317 | 66 => 331 | 67 => 337 | 68 => 347 | 69 => 349
    | 70 => 353 | 71 => 359 | 72 => 367 | 73 => 373 | 74 => 379
    | 75 => 383 | 76 => 389 | 77 => 397 | 78 => 401 | 79 => 409
    | 80 => 419 | 81 => 421 | 82 => 431 | 83 => 433 | 84 => 439
    | 85 => 443 | 86 => 449 | 87 => 457 | 88 => 461 | 89 => 463
    | 90 => 467 | 91 => 479 | 92 => 487 | 93 => 491 | 94 => 499
    | 95 => 503 | 96 => 509 | 97 => 521 | 98 => 523 | 99 => 541
    | 100 => 547 | 101 => 557 | 102 => 563 | 103 => 569 | 104 => 571
    | 105 => 577 | 106 => 587 | 107 => 593 | 108 => 599 | 109 => 601
    | 110 => 607 | 111 => 613 | 112 => 617 | 113 => 619 | 114 => 631
    | 115 => 641 | 116 => 643 | 117 => 647 | 118 => 653 | 119 => 659
    | 120 => 661 | 121 => 673 | 122 => 677 | 123 => 683 | 124 => 691
    | 125 => 701 | 126 => 709 | 127 => 719 | 128 => 727 | 129 => 733
    | 130 => 739 | 131 => 743 | 132 => 751 | 133 => 757 | 134 => 761
    | 135 => 769 | 136 => 773 | 137 => 787 | 138 => 797 | 139 => 809
    | 140 => 811 | 141 => 821 | 142 => 823 | 143 => 827 | 144 => 829
    | 145 => 839 | 146 => 853 | 147 => 857 | 148 => 859 | 149 => 863
    | 150 => 877 | 151 => 881 | 152 => 883 | 153 => 887 | 154 => 907
    | 155 => 911 | 156 => 919 | 157 => 929 | 158 => 937 | 159 => 941
    | 160 => 947 | 161 => 953 | 162 => 967 | 163 => 971 | 164 => 977
    | 165 => 983 | 166 => 991 | 167 => 997 | 168 => 1009 | 169 => 1013
    | 170 => 1019 | 171 => 1021 | 172 => 1031 | 173 => 1033 | 174 => 1039
    | 175 => 1049 | 176 => 1051 | 177 => 1061 | 178 => 1063 | 179 => 1069
    | 180 => 1087 | 181 => 1091 | 182 => 1093 | 183 => 1097 | 184 => 1103
    | 185 => 1109 | 186 => 1117 | 187 => 1123 | 188 => 1129 | 189 => 1151
    | 190 => 1153 | 191 => 1163 | 192 => 1171 | 193 => 1181 | 194 => 1187
    | 195 => 1193 | 196 => 1201 | 197 => 1213 | 198 => 1217 | 199 => 1223
    | 200 => 1229 | 201 => 1231 | 202 => 1237 | 203 => 1249 | 204 => 1259
    | 205 => 1277 | 206 => 1279 | 207 => 1283 | 208 => 1289 | 209 => 1291
    | 210 => 1297 | 211 => 1301 | 212 => 1303 | 213 => 1307 | 214 => 1319
    | 215 => 1321 | 215 => 1327 | 217 => 1361 | 218 => 1367 | 219 => 1373
    | 220 => 1381 | 221 => 1399 | 222 => 1409 | 223 => 1423 | 224 => 1427
    | 225 => 1429 | 226 => 1433 | 227 => 1439 | 228 => 1447 | 229 => 1451
    | 230 => 1453 | 231 => 1459 | 232 => 1471 | 233 => 1481 | 234 => 1483
    | 235 => 1487 | 236 => 1489 | 237 => 1493 | 238 => 1499 | 239 => 1511
    | 240 => 1523 | 241 => 1531 | 242 => 1543 | 243 => 1549 | 244 => 1553
    | 245 => 1559 | 246 => 1567 | 247 => 1571 | 248 => 1579 | 249 => 1583
    | 250 => 1597 | 251 => 1601 | 252 => 1607 | 253 => 1609 | 254 => 1613
    | 255 => 1619
    else 0
    end

  fun _low_prime_table_size(): USize => 256

  fun apply(n: A): Bool =>
    var table_index: USize = 0
    while table_index < _low_prime_table_size() do
      let prime = _low_prime_table(table_index)
      table_index = table_index + 1
      if n == prime then return true end
      if (n %% prime) == 0 then return false end
    end

    // 1 is not prime
    if n == 1 then return false end

    var i: A = 5
    while (i * i) <= n do
      if (n %% i) == 0 then return false end
      if (n %% (i + 2)) == 0 then return false end
      i = i + 6
    end
    true
