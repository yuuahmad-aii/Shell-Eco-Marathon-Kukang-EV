import sys
import pandas as pd
import matplotlib.pyplot as plt
from tkinter import Tk
from tkinter.filedialog import askopenfilename

def main():
    # Hide the main tkinter window
    root = Tk()
    root.withdraw()
    
    # If no file is provided as argument, open a file dialog
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = askopenfilename(
            title="Select Motor Log CSV File",
            filetypes=[("CSV Files", "*.csv"), ("All Files", "*.*")]
        )
        
    if not file_path:
        print("No file selected. Exiting.")
        return

    print(f"Loading data from: {file_path}")
    try:
        df = pd.read_csv(file_path)
    except Exception as e:
        print(f"Failed to read CSV: {e}")
        return

    # Convert Timestamp to datetime objects (assuming format HH:MM:SS.mmm)
    try:
        df['Timestamp'] = pd.to_datetime(df['Timestamp'], format='%H:%M:%S.%f')
    except:
        pass # If it fails, we'll just plot against index

    # Create figure with 3 subplots
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
    fig.canvas.manager.set_window_title(f"Motor Log Analysis - {file_path}")

    # Plot 1: Velocity & Target Duty
    ax1.plot(df['Timestamp'], df['Velocity (RPM)'], label='Actual RPM', color='orange')
    ax1.set_ylabel('Velocity (RPM)')
    ax1.grid(True)
    
    ax1_twin = ax1.twinx()
    ax1_twin.plot(df['Timestamp'], df['Duty Target (%)'], label='Target Duty (%)', color='blue', linestyle='--')
    ax1_twin.set_ylabel('Target Duty (%)')
    
    lines_1, labels_1 = ax1.get_legend_handles_labels()
    lines_2, labels_2 = ax1_twin.get_legend_handles_labels()
    ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper left')
    ax1.set_title('Motor Velocity & Target Command')

    # Plot 2: Mode (Control Algorithm)
    ax2.plot(df['Timestamp'], df['Mode'], label='Control Mode (0=6-Step, >0=SVPWM)', color='purple', drawstyle='steps-post')
    ax2.set_ylabel('Mode')
    ax2.set_yticks([0, 1, 2])
    ax2.set_yticklabels(['6-Step (0)', '1', 'SVPWM (2)'])
    ax2.grid(True)
    ax2.legend(loc='upper left')

    # Plot 3: Duty U, V, W
    ax3.plot(df['Timestamp'], df['Duty U (%)'], label='Phase U', color='red')
    ax3.plot(df['Timestamp'], df['Duty V (%)'], label='Phase V', color='green')
    ax3.plot(df['Timestamp'], df['Duty W (%)'], label='Phase W', color='blue')
    ax3.set_ylabel('PWM Duty Cycle (%)')
    ax3.set_xlabel('Time')
    ax3.grid(True)
    ax3.legend(loc='upper right')

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
