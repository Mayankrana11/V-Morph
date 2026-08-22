#!/usr/bin/env python3
"""
Performance tracking script for V-Morph.

This script runs benchmarks and tracks performance metrics over time,
comparing against stored baselines to detect regressions.
"""

import json
import subprocess
import sys
import os
import argparse
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional

class PerformanceTracker:
    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.baseline_file = repo_root / "performance_baseline.json"
        self.history_file = repo_root / "performance_history.json"
        
    def load_baseline(self) -> Dict:
        if self.baseline_file.exists():
            with open(self.baseline_file) as f:
                return json.load(f)
        return {}
    
    def save_baseline(self, baseline: Dict):
        with open(self.baseline_file, 'w') as f:
            json.dump(baseline, f, indent=2)
    
    def load_history(self) -> List[Dict]:
        if self.history_file.exists():
            with open(self.history_file) as f:
                return json.load(f)
        return []
    
    def save_history(self, history: List[Dict]):
        with open(self.history_file, 'w') as f:
            json.dump(history, f, indent=2)
    
    def run_benchmark(self, exe_path: Path, model_path: Path, iterations: int = 100) -> Dict:
        """Run benchmark and return metrics."""
        cmd = [
            str(exe_path),
            "--model", str(model_path),
            "--iterations", str(iterations),
            "--warmup", "10"
        ]
        
        print(f"Running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=self.repo_root)
        
        if result.returncode != 0:
            print(f"Benchmark failed: {result.stderr}")
            return {}
        
        # Parse output
        metrics = {}
        for line in result.stdout.split('\n'):
            if 'avg=' in line and 'ms' in line:
                # Parse: "Inference: avg=2.34ms, min=1.23ms, max=5.67ms"
                parts = line.split(':')
                if len(parts) == 2:
                    key = parts[0].strip().lower().replace(' ', '_')
                    vals = parts[1].strip()
                    for v in vals.split(','):
                        if '=' in v:
                            k, v = v.split('=')
                            k = k.strip()
                            v = float(v.replace('ms', '').replace('x', '').strip())
                            metrics[f"{key}_{k}"] = v
        
        return metrics
    
    def check_regression(self, current: Dict, baseline: Dict, threshold: float = 0.15) -> List[str]:
        """Check for performance regressions."""
        regressions = []
        for key, baseline_val in baseline.items():
            if key in current:
                current_val = current[key]
                # Check if current is worse than baseline by more than threshold
                if current_val > baseline_val * (1 + threshold):
                    regressions.append(
                        f"{key}: {current_val:.2f} > {baseline_val:.2f} (+{threshold*100:.0f}%)"
                    )
        return regressions
    
    def update_baseline(self, current: Dict):
        """Update baseline with current values (use max to be conservative)."""
        baseline = self.load_baseline()
        for key, val in current.items():
            if key in baseline:
                baseline[key] = max(baseline[key], val)
            else:
                baseline[key] = val
        self.save_baseline(baseline)
        print(f"Updated baseline with {len(current)} metrics")
    
    def add_to_history(self, current: Dict):
        """Add current run to history."""
        history = self.load_history()
        entry = {
            "timestamp": datetime.now().isoformat(),
            "metrics": current
        }
        history.append(entry)
        # Keep last 100 entries
        if len(history) > 100:
            history = history[-100:]
        self.save_history(history)
    
    def print_report(self, current: Dict, baseline: Dict, regressions: List[str]):
        print("\n" + "="*60)
        print("PERFORMANCE REPORT")
        print("="*60)
        print(f"Timestamp: {datetime.now().isoformat()}")
        print()
        
        print("Current Metrics:")
        for key, val in sorted(current.items()):
            baseline_val = baseline.get(key, 0)
            diff_pct = ((val - baseline_val) / baseline_val * 100) if baseline_val > 0 else 0
            status = "⚠️" if diff_pct > 15 else "✓"
            print(f"  {key}: {val:.2f} (baseline: {baseline_val:.2f}, diff: {diff_pct:+.1f}%) {status}")
        
        print()
        if regressions:
            print("REGRESSIONS DETECTED:")
            for r in regressions:
                print(f"  ❌ {r}")
        else:
            print("✅ No regressions detected")
        print("="*60)

def main():
    parser = argparse.ArgumentParser(description="V-Morph Performance Tracker")
    parser.add_argument("--exe", required=True, help="Path to benchmark executable")
    parser.add_argument("--model", required=True, help="Path to test model")
    parser.add_argument("--iterations", type=int, default=100, help="Benchmark iterations")
    parser.add_argument("--threshold", type=float, default=0.15, help="Regression threshold (0.15 = 15%)")
    parser.add_argument("--update-baseline", action="store_true", help="Update baseline with current results")
    parser.add_argument("--repo-root", default=".", help="Repository root path")
    
    args = parser.parse_args()
    
    repo_root = Path(args.repo_root).resolve()
    tracker = PerformanceTracker(repo_root)
    
    exe_path = Path(args.exe).resolve()
    model_path = Path(args.model).resolve()
    
    if not exe_path.exists():
        print(f"Error: Executable not found: {exe_path}")
        sys.exit(1)
    
    if not model_path.exists():
        print(f"Error: Model not found: {model_path}")
        sys.exit(1)
    
    print(f"Running performance benchmark...")
    print(f"  Executable: {exe_path}")
    print(f"  Model: {model_path}")
    print(f"  Iterations: {args.iterations}")
    
    # Run benchmark
    current = tracker.run_benchmark(exe_path, model_path, args.iterations)
    
    if not current:
        print("Benchmark failed to produce metrics")
        sys.exit(1)
    
    # Load baseline
    baseline = tracker.load_baseline()
    
    # Check for regressions
    regressions = tracker.check_regression(current, baseline, args.threshold)
    
    # Print report
    tracker.print_report(current, baseline, regressions)
    
    # Update history
    tracker.add_to_history(current)
    
    # Update baseline if requested
    if args.update_baseline:
        tracker.update_baseline(current)
    
    # Exit with error if regressions found
    if regressions:
        print("\n❌ Performance regression detected!")
        sys.exit(1)
    else:
        print("\n✅ All performance checks passed!")
        sys.exit(0)

if __name__ == "__main__":
    main()