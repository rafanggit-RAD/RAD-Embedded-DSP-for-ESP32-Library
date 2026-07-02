import re
import sys
import json
import os

try:
    import google.generativeai as genai
    HAS_GENAI = True
except ImportError:
    HAS_GENAI = False

def print_header():
    print("======================================================")
    print("       RadAnalyzer - Static DSP Load Estimator        ")
    print("======================================================\n")

def analyze_ino(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading file {filepath}: {e}")
        return

    # 1. Parse Routing
    route_pattern = re.compile(r'//\s*@Route:\s*(.*)')
    routes = route_pattern.findall(content)
    
    # 2. Extract JSON Schema from INO
    schema_pattern = re.compile(r'const char\*\s*dspSchema\s*=\s*"({.*?})";', re.DOTALL)
    schema_match = schema_pattern.search(content)
    
    schema = {}
    if schema_match:
        try:
            raw_json = schema_match.group(1).replace('\\"', '"')
            schema = json.loads(raw_json)
        except Exception as e:
            print(f"Error parsing dspSchema JSON: {e}")
            
    modules = schema.get("modules", {})
    if not modules:
        print("No modules found in schema. Please run RadScanner.py first to inject the schema!")
        return

    # DSP Cost Table (ESP32 FPU estimates in % per block at 48kHz / 128 samples)
    cost_table = {
        "Biquad": 0.5,
        "Gain": 0.1,
        "Mixer2": 0.3,
        "Dynamics": 1.2,
        "FIR": 15.0 # Assuming ~128 taps
    }

    core0_load = 0.0
    core1_load = 0.0
    
    core0_modules = []
    core1_modules = []
    unknown_modules = []
    
    # Simple regex to check if process is in Core 1 task or Core 0 task
    core1_block_match = re.search(r'dualCore\.process\s*\(\s*\[&\]\(\)\s*\{(.*?)\}', content, re.DOTALL)
    core1_text = core1_block_match.group(1) if core1_block_match else ""
    
    for mod_id, mod in modules.items():
        name = mod.get("name")
        mtype = mod.get("type")
        cost = cost_table.get(mtype, 1.0)
        
        # Check where it's processed
        if f"{name}.process" in core1_text:
            core1_load += cost
            core1_modules.append(name)
        elif f"{name}.process" in content:
            core0_load += cost
            core0_modules.append(name)
        else:
            unknown_modules.append(name)

    print(f"Found {len(modules)} DSP Modules.")
    print(f"Found {len(routes)} Routing Paths.\n")
    
    print("--- ESTIMATED CPU LOAD ---")
    print(f"Core 0 Estimated Load: {core0_load:.1f} %")
    print(f"  Modules: {', '.join(core0_modules) if core0_modules else 'None'}")
    
    print(f"\nCore 1 Estimated Load: {core1_load:.1f} %")
    print(f"  Modules: {', '.join(core1_modules) if core1_modules else 'None'}")
    
    if unknown_modules:
        print(f"\nModules configured but NOT processed (Dead Code?): {', '.join(unknown_modules)}")

    print("\n------------------------------------------------------")
    if core0_load > 85.0 or core1_load > 85.0:
        print("⚠️ WARNING: CPU load is getting high. Risk of I2S DMA Starvation (Crackling audio)!")
    else:
        print("✅ CPU Load is safe. Lots of headroom available!")

    # Ask AI for patterns if API key is set
    print("\n======================================================")
    print("               AI Pattern Analyzer                    ")
    print("======================================================")
    api_key = os.environ.get("GEMINI_API_KEY")
    if HAS_GENAI and api_key:
        print("Contacting Gemini AI for code review and optimization suggestions...\n")
        genai.configure(api_key=api_key)
        try:
            model = genai.GenerativeModel('gemini-1.5-flash')
            prompt = f"""
            You are an expert embedded DSP audio engineer. Analyze this ESP32 C++ audio processing code.
            Look for optimization opportunities:
            1. Are there too many Biquads in series that could be optimized?
            2. Is the dual-core fork-join architecture balanced? Core 0 load is {core0_load}%, Core 1 is {core1_load}%.
            3. Any unused variables, bad routing, or code smells?
            Give 3 short, punchy bullet points of advice. Use English.
            
            Code:
            {content}
            """
            response = model.generate_content(prompt)
            print(response.text)
        except Exception as e:
            print(f"AI Analysis Failed: {e}")
    else:
        print("INFO: Set GEMINI_API_KEY environment variable and install 'google-generativeai' for deep AI code review!")
        print("\n--- Basic Heuristic Analysis ---")
        
        # 1. Biquad Chaining Check
        biquad_count = sum(1 for m in modules.values() if m["type"] == "Biquad")
        if biquad_count > 6:
            print("💡 Suggestion: You have a lot of Biquads! If they are chained (e.g. 8-band EQ), consider converting them into a single FIR filter for linear phase, or ensure they are properly balanced across Cores.")
            
        # 2. Core Balancing
        if abs(core0_load - core1_load) > 5.0 and core1_load > 0:
            print("💡 Suggestion: Your Dual-Core load is unbalanced. Try moving some processing blocks from the heavier core to the lighter core inside the `dualCore.process()` lambda blocks.")
            
        # 3. Unused Modules
        if unknown_modules:
            print(f"🧹 Clean Up: Remove {', '.join(unknown_modules)} from `dspControl.attach()` if you aren't calling `.process()` on them to save RAM.")
            
        if biquad_count <= 6 and abs(core0_load - core1_load) <= 5.0 and not unknown_modules:
            print("✨ Code looks clean and well-optimized heuristically!")

if __name__ == "__main__":
    print_header()
    if len(sys.argv) > 1:
        analyze_ino(sys.argv[1])
    else:
        print("Usage: python RadAnalyzer.py <path_to_ino_file>")
        print("Example: python RadAnalyzer.py ../examples/Master_Passthrough/Master_Passthrough.ino")
