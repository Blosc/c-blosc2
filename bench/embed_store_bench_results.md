# EmbedStore benchmark

The following parameters were set by the program arguments:
- 100000 keys
- 8 bytes/value
- 5 reps

```
---------------------------------------------------------------------
operation     calls     total (ms)        ns/op        ops/s      MB/s
---------------------------------------------------------------------
set          100000      14543.719     145437.2         6876       0.1
get          100000        392.950       3929.5       254485       2.0
serialize         5         20.453    4090505.2          244         -
remove       100000         14.272        142.7      7006852         -
open              5         67.772   13554423.8           74         -
```

## CPU Parameters

The specifications for the CPU were obtained by running `lscpu`.

11th Gen Intel(R) Core(TM) i5-1155G7 2.50GHz.

Number of cores: 8.

Cache parameters:
- L1d: 192 KiB (4 instances)
- L1i: 128 KiB (4 instances)
- L2:  5 MiB (4 instances)
- L3:  8 MiB (1 instance)

# RAM Parameters
16 GB DDR4. 
