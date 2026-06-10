#include "commands.h"
#include "pins.h"

String process_command(String command) {
    command.trim(); 
    
    if (command == "HELLO") {
        return "banana confirmed sir.";
    } 
    else if (command == "ON_Y") {
        digitalWrite(LED_Y, HIGH);
        return "LED_ON_OK";
    } 
    else if (command == "OFF_Y") {
        digitalWrite(LED_Y, LOW);
        return "LED_OFF_OK";
    }
    else if (command == "ON_O") {
        digitalWrite(LED_O, HIGH);
        return "LED_O_ON_OK";
    }
    else if (command == "OFF_O") {
        digitalWrite(LED_O, LOW);
        return "LED_O_OFF_OK";
    }
    

    return "UNKNOWN_CMD: " + command;
}