Trade Matching Engine In C++.

benchmarks :

```
================ BENCHMARK FOR : Tick To Trade Time ================


 p50 : 1075 cycles  (430 ns)
 p75 : 1146 cycles  (459 ns)
 p90 : 1256 cycles  (503 ns)
 p99 : 3866 cycles  (1548 ns)

====================================================================


================ BENCHMARK FOR : Matching Engine Processing Time ================


 p50 : 185 cycles  (74 ns)
 p75 : 232 cycles  (92 ns)
 p90 : 299 cycles  (119 ns)
 p99 : 579 cycles  (231 ns)

====================================================================


================ BENCHMARK FOR : Queue Wait Time ================


 p50 : 264 cycles  (105 ns)
 p75 : 274 cycles  (109 ns)
 p90 : 350 cycles  (140 ns)
 p99 : 2391 cycles  (957 ns)

====================================================================


================ BENCHMARK FOR : ME Throughput (time between consecutive reads) ================


 p50 : 1206 cycles  (483 ns)
 p75 : 1275 cycles  (510 ns)
 p90 : 1341 cycles  (537 ns)
 p99 : 3140 cycles  (1258 ns)

====================================================================


================ BENCHMARK FOR : Order Gateway Processing Time ================


 p50 : 286 cycles  (114 ns)
 p75 : 318 cycles  (127 ns)
 p90 : 343 cycles  (137 ns)
 p99 : 437 cycles  (175 ns)

====================================================================


~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ CLOSING CAPITOL ~~~~~~~~~~~~~~~~~~ 
```

Orders Processed Per Second ~ 2.3 Million orders/S at p50, 2 Million orders/S at p90.