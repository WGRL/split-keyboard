#!/bin/bash

FOLDER="${1:-.}"
OUTPUT="${2:-output.txt}"

# Czyścimy plik wynikowy
> "$OUTPUT"

find "$FOLDER" -type f ! -path "./$OUTPUT" ! -name "$(basename "$OUTPUT")" -print0 |
while IFS= read -r -d '' file; do
    {
        echo ""
        echo "============================================================"
        echo "PLIK: $file"
        echo "============================================================"
        echo ""
        cat "$file"
        echo ""
        echo ""
    } >> "$OUTPUT"
done

echo "Gotowe. Zapisano do: $OUTPUT"
```
