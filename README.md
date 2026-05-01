# ESP32-BT-Speaker-TTGO-TDisplay
Bluetooth speaker with TTGO T-Display (Non-S3)

## How to make
1. Required parts are: TTGO or LILYGO T-Display, (the version WITHOUT S3 chip! S3 chip does not support BT Classic and cannot be used.), and MAX98357A module with this pinout (Sparkfun, MH, etc.) - LRC, BCLK, DIN, GAIN, SD, GND, VIN, a small speaker
2. Remove centermost pin from the header (GAIN pin)
3. Solder the pin header to the MAX98357A module (The plastic block must be on top of the module where the pin ID writing is, not the other way around)
4. Align the pin header to the lower right side of the display (where the reset button is), then solder it (VIN and GND pin MUST be aligned properly, otherwise the chip will burn)
5. Solder to the board, careful to adjust board angle to prevent shorting
6. Connect to computer
7. Find device in Device Manager
8. Setup TFT_eSPI to use pin definitions for TTGO T-Display (number 25)
9. Open the .ino file
10. Upload with partition scheme "Huge App" (other schemes will fail)

## How to change name, prompt sound, boot image, menu image
- Name: Go to `a2dp_sink.start("....");` and change what is between the quotes
- Prompt sound: Prepare sound less than 2.5 seconds at 44100Hz with Stereo channels, then export it as "Headerless PCM Audio" or "SND", put at least `poweron.snd`, `connected.snd` and `disconnected.snd` in a folder, copy over `MakeSound.py` to the folder, then run `python MakeSound.py` which will generate the sound header. Replace the stock sound header `riffsound2.h` with the new version.
- Boot image, menu image: Use LCD Image Converter, image size 240x135, set Options - Conversion... - Preset to "Color R5G6B5", go to Image tab, set Block size to 16 bit, do not turn on RLE Compression, then click Show Preview to get the data. Replace "mainmenu" section with it. Do the same to "bootimg" section for the boot image.

## Known issues
- Certain signal sources will cause the audio to glitch or skip over time
- Screen flashes as it boots up
