# How to run
1. ```make```
2. ```make -f make_make```
3. ```./main```

if you get error 

`Makefile:6: *** missing separator (did you mean TAB instead of 8 spaces?).  Stop.`

Then, 

```vim [the makefile]```

those intended line, erase them all

then replace with TAB line



# Info about GPIO

On raspberry pi, run

```gpio readall```

on the BCM colume, that's the GPIO im using

BCM 15 switch (cannot use LED on this pin, because `gpio readall` says this is Tx/Rx)

BCM 23 LED

# Info
i tried changing gpio 15 to output

after running, it will be changed back to input

maybe ur code changes it
