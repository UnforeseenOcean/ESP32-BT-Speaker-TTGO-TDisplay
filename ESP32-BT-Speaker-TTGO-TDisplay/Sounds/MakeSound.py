import re
import os
import sys

def bytes_to_c_arr(data):
    # Formats bytes into 0x00 style strings
    return [format(b, '#04x') for b in data]

def write_header_content(output_file, sound_files):
    with open(output_file, "w") as f:
        
        for file_path in sound_files:
            if not os.path.exists(file_path):
                print(f"Warning: {file_path} not found. Skipping...")
                continue
            
            var_name = os.path.splitext(os.path.basename(file_path))[0]
            
            with open(file_path, "rb") as rf:
                data = rf.read()
            
            print(f"Processing {file_path} ({len(data)} bytes)...")
            
            # 2. Write the Array
            f.write(f"const unsigned char {var_name}[] = {{\n")
            
            # Convert to hex strings
            hex_data = bytes_to_c_arr(data)
            
            # Join with commas and wrap lines every 12 elements for readability
            for i in range(0, len(hex_data), 12):
                line = ", ".join(hex_data[i:i+12])
                f.write(f"  {line},\n")
            
            f.write("};\n")
            
            # 3. Write the Length
            f.write(f"unsigned long {var_name}_len = {len(data)};\n\n")

# List of files to process
target_files = [
    "poweron.snd", 
    "volmax.snd", 
    "volmin.snd", 
    "connected.snd", 
    "disconnected.snd"
]

write_header_content("riffsound2.h", target_files)
print("Generated new sound data")