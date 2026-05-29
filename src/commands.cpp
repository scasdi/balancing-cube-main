#include "commands.h"
#include "pins.h" // כדי להכיר את החומרה

String process_command(String command) {
    command.trim(); // ניקוי רווחים מיותרים
    
    // כאן יושבת כל הלוגיקה - פעם אחת בלבד!
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
    
    // אם אף תנאי לא התקיים
    return "UNKNOWN_CMD: " + command;
}