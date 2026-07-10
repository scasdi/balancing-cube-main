import os

OUTPUT_FILE = "all_code.txt"
# התיקיות שאנחנו רוצים להיכנס אליהן מתוך השורש
ALLOWED_DIRS = {"src", "include", "scripts"}

with open(OUTPUT_FILE, "w", encoding="utf-8") as outfile:
    for root, dirs, files in os.walk("."):
        
        # אם אנחנו בתיקיית השורש, נשאיר ברשימה רק את התיקיות המורשות
        # פעולה זו גורמת ל-os.walk להתעלם לחלוטין מכל שאר התיקיות (כמו .pio, lib וכו')
        if root == ".":
            dirs[:] = [d for d in dirs if d in ALLOWED_DIRS]
            
        for file in files:
            # מדלג על קובץ הפלט עצמו כדי שלא ישכפל את עצמו
            if file == OUTPUT_FILE:
                continue
                
            # סינון לפי סוגי קבצים (הוספתי .ini עבור platformio.ini ו-.c ליתר ביטחון)
            if file.endswith((".cpp", ".c", ".h", ".py", ".json", ".ini")):
                filepath = os.path.join(root, file)
                outfile.write(f"\n\n{'='*50}\nFILE: {filepath}\n{'='*50}\n")
                try:
                    with open(filepath, "r", encoding="utf-8") as infile:
                        outfile.write(infile.read())
                except Exception as e:
                    outfile.write(f"Error reading file: {e}\n")
                    
print(f"Created {OUTPUT_FILE} successfully! Upload this file.")