# ATMEGA328 Backpropagation neural network

This directory contains simple code interpreter for ATMEGA328/Arduino AI.

Using bpnn library
https://github.com/dariuszrorat/bpnn


### MMI codes

    MMI codes:

      *00*#               repeat the last code
      *11*XX*YY*ZZ#       set input XX, hidden YY, ZZ layers before NN creation
      *12*XX#             set pattern count before nn creation
      *21*XX#             set learning rate divided by 100
      *22*XX#             set momentum divided by 100
      *23*XX#             set max epochs
      *24*XX#             set desired error divided by 1000000
      *31*XX*YY           assign XX pin to YY input neuron number, YY = 0..neurons-1
      *32*XX*YY           assign XX pin to YY output neuron number, YY = 0..neurons-1
      *33*XX*YY*ZZ#       set pattern at XX index from YY pin and ZZ value if NN created
      *34*XX*YY*ZZ#       set target at XX index from YY pin and ZZ value if NN created
      *41*#               create neural network
      *42*#               start neural network training, press any key to abort
      *43*#               enable neural network calculation
      *44*#               disable neural network calculation
      *51*#               save weights to EEPROM
      *52*#               load weights from EEPROM

### Example

XOR problem

Architecture:
  input neurons: 2
  hidden neurons: 4
  output neurons: 1
  learning rate: 0.5
  momentum: 0.1
  max epochs: 1000
  desired error: 0.01

```

*11*2*4*1#
*12*4#
*21*50#
*22*10#
*23*1000#
*24*10000#
*31*14*0#
*31*15*1#
*32*9*0#
*33*0*0*0#
*33*0*1*0#
*33*1*0*0#
*33*1*1*1023#
*33*2*0*1023#
*33*2*1*0#
*33*3*0*1023#
*33*3*1*1023#
*34*0*0*0#
*34*1*0*255#
*34*2*0*255#
*34*3*0*0#
*41*#
*42*#
*43*#


```
