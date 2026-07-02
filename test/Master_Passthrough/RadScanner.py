import os
import glob
import re
import json

def get_params_for_type(module_type):
    """Mengembalikan daftar parameter lengkap sesuai configParam untuk setiap tipe blok DSP."""
    # Bersihkan template parameter: "MatrixRouter<3,2>" -> "MatrixRouter"
    base_type = module_type.split("<")[0].strip()
    
    if base_type == "Biquad":
        return [
            "0: Filter Type",
            "1: Frequency (Hz)",
            "2: Gain (dB)",
            "3: Q-Factor",
            "100: Bypass (0/1)"
        ]
    elif base_type == "Dynamics":
        return [
            "0: Dynamics Type",
            "1: Threshold (dB)",
            "2: Ratio",
            "3: Attack (ms)",
            "4: Hold (ms)",
            "5: Release (ms)",
            "6: Makeup Gain (dB)",
            "7: SC Filter Type",
            "8: SC Freq (Hz)",
            "100: Bypass (0/1)"
        ]
    elif base_type == "Mixer":
        # Ekstrak N dari Mixer<N>
        m = re.search(r'<(\d+)>', module_type)
        n = int(m.group(1)) if m else 2
        params = [f"{i}: Gain In{i} (dB)" for i in range(n)]
        params += [f"{100+i}: Mute In{i} (0/1)" for i in range(n)]
        return params
    elif base_type == "Splitter":
        m = re.search(r'<(\d+)>', module_type)
        n = int(m.group(1)) if m else 2
        params = [f"{i}: Gain Out{i} (dB)" for i in range(n)]
        params += [f"{100+i}: Mute Out{i} (0/1)" for i in range(n)]
        return params
    elif base_type == "Gain":
        return [
            "0: Gain (dB)",
            "1: Mute (0/1)",
            "2: Phase Invert (0/1)"
        ]
    elif base_type == "FIR":
        return [
            "3: Taps Number (16-512)",
            "4: Gain (dB)",
            "100: Bypass (0/1)"
        ]
    elif base_type == "MatrixRouter":
        # Ekstrak IN, OUT dari MatrixRouter<IN,OUT>
        m = re.search(r'<(\d+)\s*,\s*(\d+)>', module_type)
        num_in, num_out = (int(m.group(1)), int(m.group(2))) if m else (3, 2)
        params = []
        for i in range(num_in):
            for j in range(num_out):
                idx = i * num_out + j
                params.append(f"{idx}: In{i}->Out{j} (Lin)")
        return params
    elif base_type == "Meter":
        return [
            "0: Level (dB)",
            "1: Decay Factor"
        ]
    else:
        return ["0: Custom Value"]

def scan_ino_files():
    ino_files = glob.glob("*.ino")
    if not ino_files:
        print("Error: Tidak ada file .ino yang ditemukan di folder ini!")
        print("Pastikan Anda memindahkan dan menjalankan script ini di dalam folder proyek Arduino Anda.")
        return
        
    target_ino = ino_files[0]
    print(f"Memindai kode sumber: {target_ino}\n")
    
    try:
        with open(target_ino, 'r', encoding='utf-8') as f:
            content = f.read()
            
        # ================================================================
        # TAHAP 1: Pemindaian Deklarasi Tipe
        # ================================================================
        # Mendukung:
        #   RadDSP::Biquad eqBand1;
        #   RadDSP::Biquad eqL, eqR;
        #   RadDSP::Mixer<2> mixerL, mixerR;
        #   RadDSP::MatrixRouter<3, 2> routerL, routerR;
        decl_pattern = re.compile(
            r'RadDSP::([a-zA-Z0-9_]+(?:<[^>]*>)?)\s+'   # Tipe (termasuk template <...>)
            r'([a-zA-Z0-9_]+(?:\s*,\s*[a-zA-Z0-9_]+)*)'  # Nama variabel (bisa lebih dari satu dipisahkan koma)
            r'\s*;'
        )
        declarations = {}
        
        decl_matches = decl_pattern.findall(content)
        for match in decl_matches:
            mod_type = match[0].strip()    # e.g. "Biquad" atau "Mixer<2>"
            var_names = match[1].strip()   # e.g. "eqL, eqR"
            
            # Pisahkan multi-deklarasi: "eqL, eqR" -> ["eqL", "eqR"]
            for var_name in [v.strip() for v in var_names.split(",")]:
                if var_name:
                    declarations[var_name] = mod_type
                    print(f" [TAHAP 1] Deklarasi Ditemukan -> Variabel: {var_name} | Tipe: RadDSP::{mod_type}")
            
        print("\n")
            
        # ================================================================
        # TAHAP 2: Pemindaian Registrasi ID
        # ================================================================
        # Contoh: dspControl.attach(1, &eqBand1);
        attach_pattern = re.compile(r'attach\s*\(\s*(\d+)\s*,\s*&([a-zA-Z0-9_]+)\s*\)')
        module_map = {}
        
        attach_matches = attach_pattern.findall(content)
        for match in attach_matches:
            module_id = match[0]
            module_name = match[1]
            module_type = declarations.get(module_name, "Unknown")
            
            module_map[module_id] = {
                "name": module_name,
                "type": module_type,
                "params": get_params_for_type(module_type)
            }
            
            print(f" [TAHAP 2] Registrasi ID {module_id} -> {module_name} ({module_type})")
            
        if not module_map:
            print("\nPeringatan: Tidak ada perintah 'attach' yang terdeteksi dalam kode INO ini.")

        # ================================================================
        # TAHAP 3: Pemindaian Routing (dari komentar @Route)
        # ================================================================
        # Contoh: // @Route: I2S_In -> EqI2S_0_L -> EqI2S_1_L -> CompI2S_L -> mixerL
        routing_edges = []
        route_pattern = re.compile(r'//\s*@Route:\s*(.+)')
        route_matches = route_pattern.findall(content)
        
        for route_line in route_matches:
            nodes = [n.strip() for n in route_line.split("->")]
            for i in range(len(nodes) - 1):
                edge = [nodes[i], nodes[i+1]]
                if edge not in routing_edges:
                    routing_edges.append(edge)
            print(f" [TAHAP 3] Route: {' -> '.join(nodes)}")
            
        # ================================================================
        # TAHAP 4: Menyimpan output JSON
        # ================================================================
        output = {
            "routing": routing_edges,
            "modules": module_map
        }
        
        with open("dsp_config.json", "w", encoding='utf-8') as f:
            json.dump(output, f, indent=2)
            
        # ================================================================
        # TAHAP 5: Membuat berkas header dsp_schema.h di folder yang sama
        # ================================================================
        # Konversi JSON schema ke string satu baris yang sesuai format konstanta C
        json_minimized = json.dumps(output)
        
        # Format string C++ dspSchema
        # Memisahkan schema menjadi baris-baris berukuran sedang agar rapi di editor
        chunk_size = 100
        schema_lines = []
        for idx in range(0, len(json_minimized), chunk_size):
            chunk = json_minimized[idx:idx+chunk_size].replace('"', '\\"')
            schema_lines.append(f'  "{chunk}"')
        
        header_content = (
            "#ifndef DSP_SCHEMA_H\n"
            "#define DSP_SCHEMA_H\n\n"
            "// AUTO GENERATED BY RADSCANNER - DO NOT EDIT MANUALLY\n"
            "const char* dspSchema = \n" + "\n".join(schema_lines) + ";\n\n"
            "#endif // DSP_SCHEMA_H\n"
        )
        
        header_filename = "dsp_schema.h"
        with open(header_filename, 'w', encoding='utf-8') as f_header:
            f_header.write(header_content)
        print(f" [TAHAP 5] SUKSES: Berkas header '{header_filename}' berhasil dibuat!")

        print(f"\n{'='*50}")
        print(f"SUKSES! Terdeteksi {len(module_map)} modul, {len(routing_edges)} koneksi routing.")
        print(f"File 'dsp_config.json' dan '{header_filename}' telah diperbarui.")
        print(f"{'='*50}")
        
    except Exception as e:
        print(f"Terjadi kesalahan fatal saat memproses file: {str(e)}")

if __name__ == "__main__":
    print("===========================================")
    print("   RadScanner v2.0 - INO to JSON Schema    ")
    print("===========================================\n")
    scan_ino_files()
    input("\nTekan Enter untuk keluar...")
