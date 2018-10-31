# LASER SECURITY: An Arduino Project
LaserSec: A security module using Arduino which receives SMS as commands and sends SMS as alerts when the laser that hits the light-sensitive module is interrupted.

# Introduction
**Light Amplification by Simulated Emission of
Radiation.** - A device that generates an intense beam of coherent
monochromatic light. This project utilizes a continuous LASER
beam pointed towards a LDR (Light Dependent Resistor) to
determine if an area has not been trespassed. A piezo-buzzer
generates noisy tones to indicate that the LASER in contact with
the LDR has been interrupted. A SIM800L GSM Module pushes
notifications via text and can receive commands to activate or
deactivate features of the security system. An LCD constantly
shows status messages of the system’s current state and actions
successfully done from SMS commands.

# Running the project
## Further information
Please see the `Technical-Paper.pdf` file for more information such as wiring diagrams, flowcharts, and related discussions. 

The `arduino-sketch.ino` contains the source code that may be imported to the Arduino IDE.