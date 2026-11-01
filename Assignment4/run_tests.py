#!/usr/bin/env python3
import subprocess
import time
import os
import hashlib

def create_file(path, size_bytes, fill_byte=None, pattern=None):
    with open(path, "wb") as f:
        if fill_byte is not None:
            f.write(bytes([fill_byte]) * size_bytes)
        elif pattern is not None:
            repeats = (size_bytes // len(pattern)) + 1
            data = (pattern * repeats)[:size_bytes]
            f.write(data)
        else:
            f.write(os.urandom(size_bytes))

def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(8192):
            h.update(chunk)
    return h.hexdigest()

def kill_leftovers():
    subprocess.run(["pkill", "-9", "-f", "./server"], stderr=subprocess.DEVNULL)
    subprocess.run(["pkill", "-9", "-f", "./receiver"], stderr=subprocess.DEVNULL)
    subprocess.run(["pkill", "-9", "-f", "./sender"], stderr=subprocess.DEVNULL)
    time.sleep(0.5)

def run_experiment(name, n_stations, sender_configs, log_prefix, port=5000):
    kill_leftovers()
    os.makedirs(f"logs/{log_prefix}", exist_ok=True)
    env = {**os.environ, "CDMA_PORT": str(port)}
    
    server_log = open(f"logs/{log_prefix}/server.log", "w")
    recv_log = open(f"logs/{log_prefix}/receiver.log", "w")
    
    srv_proc = subprocess.Popen(["./server", str(n_stations)], stdout=server_log, stderr=subprocess.STDOUT, env=env)
    time.sleep(0.3)
    if srv_proc.poll() is not None:
        raise RuntimeError(f"Server failed to start in {name}! Check {log_prefix}/server.log")
        
    rcv_proc = subprocess.Popen(["./receiver", "0"], stdout=recv_log, stderr=subprocess.STDOUT, env=env)
    time.sleep(0.3)
    if rcv_proc.poll() is not None:
        raise RuntimeError(f"Receiver failed to start in {name}! Check {log_prefix}/receiver.log")
        
    sender_procs = []
    sender_logs = []
    for idx, (st_name, in_file) in enumerate(sender_configs):
        slog = open(f"logs/{log_prefix}/sender_{idx+1}.log", "w")
        sender_logs.append(slog)
        sproc = subprocess.Popen(["./sender", st_name, in_file], stdout=slog, stderr=subprocess.STDOUT, env=env)
        sender_procs.append(sproc)
        time.sleep(0.1)
        
    for p in sender_procs:
        p.wait()
    rcv_proc.wait()
    srv_proc.wait()
    
    server_log.close()
    recv_log.close()
    for l in sender_logs:
        l.close()
    time.sleep(0.3)
    print(f"[{name}] Completed successfully on port {port}.")

def main():
    os.makedirs("test_data", exist_ok=True)
    os.makedirs("logs", exist_ok=True)
    
    # -------------------------------------------------------------
    # Test Case 1: Non-Power-of-2 Stations (Matrix Expansion & Zero-Padding)
    # n = 3, forcing N = 4
    # -------------------------------------------------------------
    print("=== Running Test Case 1: Non-Power-of-2 Stations (n=3, N=4) ===")
    create_file("test_data/tc1_input.txt", 46 * 10, pattern=b"TestData_For_TC1_Station_Payload_Frame_Content!!")
    run_experiment(
        "Test Case 1",
        3,
        [("Station_1", "test_data/tc1_input.txt"),
         ("Station_2", "test_data/tc1_input.txt"),
         ("Station_3", "test_data/tc1_input.txt")],
        "tc1",
        port=5001
    )
    
    # -------------------------------------------------------------
    # Test Case 2: Unbalanced Payload Lengths (Dynamic Silence)
    # Station_1: 10 KB (~220 frames), Station_2: 1 KB (~22 frames)
    # -------------------------------------------------------------
    print("=== Running Test Case 2: Unbalanced Payload Lengths ===")
    create_file("test_data/tc2_10k.txt", 46 * 220, pattern=b"Station1_10KB_Stream_Data_Pattern_Verification01")
    create_file("test_data/tc2_1k.txt", 46 * 22, pattern=b"Station2_1KB_Short_Stream_Verify_Silence_Slot_01")
    run_experiment(
        "Test Case 2",
        2,
        [("Station_1", "test_data/tc2_10k.txt"),
         ("Station_2", "test_data/tc2_1k.txt")],
        "tc2",
        port=5002
    )
    
    # -------------------------------------------------------------
    # Test Case 3: Complementary Bit Pattern Stress
    # All Ones (0xFF), All Zeros (0x00), Alternating (0xFF vs 0x00)
    # -------------------------------------------------------------
    print("=== Running Test Case 3: Complementary Bit Pattern Stress ===")
    create_file("test_data/tc3_ones.txt", 46 * 5, fill_byte=0xFF)
    create_file("test_data/tc3_zeros.txt", 46 * 5, fill_byte=0x00)
    
    # Subtest 3A: All Ones across 4 stations
    run_experiment(
        "Test Case 3A (All Ones)",
        4,
        [("Station_1", "test_data/tc3_ones.txt"),
         ("Station_2", "test_data/tc3_ones.txt"),
         ("Station_3", "test_data/tc3_ones.txt"),
         ("Station_4", "test_data/tc3_ones.txt")],
        "tc3_ones",
        port=5003
    )
    
    # Subtest 3B: All Zeros across 4 stations
    run_experiment(
        "Test Case 3B (All Zeros)",
        4,
        [("Station_1", "test_data/tc3_zeros.txt"),
         ("Station_2", "test_data/tc3_zeros.txt"),
         ("Station_3", "test_data/tc3_zeros.txt"),
         ("Station_4", "test_data/tc3_zeros.txt")],
        "tc3_zeros",
        port=5004
    )
    
    # Subtest 3C: Alternating (Station 1: 0xFF, Station 2: 0x00)
    run_experiment(
        "Test Case 3C (Alternating)",
        2,
        [("Station_1", "test_data/tc3_ones.txt"),
         ("Station_2", "test_data/tc3_zeros.txt")],
        "tc3_alternating",
        port=5005
    )
    
    # -------------------------------------------------------------
    # Test Case 4: Complete File Integrity Verification (Hash / Diff)
    # 4 stations with distinct substantial payloads
    # -------------------------------------------------------------
    print("=== Running Test Case 4: Complete File Integrity Verification ===")
    for i in range(1, 5):
        pat = f"PayloadContent_For_Station_{i}_DataLink_CDMA_Verification_Text_Block_{i}!".encode('ascii')
        create_file(f"input_station_{i}.txt", 46 * 35, pattern=pat)
        if os.path.exists(f"output_station_{i}.txt"):
            os.remove(f"output_station_{i}.txt")
    
    run_experiment(
        "Test Case 4",
        4,
        [("Station_1", "input_station_1.txt"),
         ("Station_2", "input_station_2.txt"),
         ("Station_3", "input_station_3.txt"),
         ("Station_4", "input_station_4.txt")],
        "tc4",
        port=5006
    )
    
    print("\n--- Test Case 4 Verification Checks ---")
    for i in range(1, 5):
        inp = f"input_station_{i}.txt"
        out = f"output_station_{i}.txt"
        diff_res = subprocess.run(["diff", inp, out], capture_output=True, text=True)
        md5_in = md5_file(inp)
        md5_out = md5_file(out)
        sha_in = sha256_file(inp)
        sha_out = sha256_file(out)
        match = (diff_res.returncode == 0) and (md5_in == md5_out) and (sha_in == sha_out)
        print(f"Station_{i}: Diff Exit Code = {diff_res.returncode} | MD5 Match: {md5_in == md5_out} ({md5_in}) | SHA-256 Match: {sha_in == sha_out} | Perfect: {match}")
    
    # -------------------------------------------------------------
    # Test Case 5: Higher Spreading Factor Scaling (N = 8)
    # Server with 5 stations -> Expands to N = 8 (520 * 8 = 4160 chips/frame)
    # -------------------------------------------------------------
    print("\n=== Running Test Case 5: Higher Spreading Factor Scaling (N = 8) ===")
    for i in range(1, 6):
        pat = f"HighSpreadingFactor_N8_Stream_{i}_SylvesterExpansion_WalshCode_Payload!".encode('ascii')
        create_file(f"test_data/tc5_st{i}.txt", 46 * 20, pattern=pat)
        
    run_experiment(
        "Test Case 5",
        5,
        [(f"Station_{i}", f"test_data/tc5_st{i}.txt") for i in range(1, 6)],
        "tc5",
        port=5007
    )
    print("All test cases completed successfully!")

if __name__ == "__main__":
    main()
