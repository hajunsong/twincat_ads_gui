#!/usr/bin/env python3
"""
TwinCAT 로깅 CSV(M0~M30)에서 nActualPosition/nActualTorque를 읽어 그래프로 저장합니다.

기본 동작: 프로젝트 루트 기준 logging/ 아래 수정 시각이 가장 최근인 세션 폴더를 사용합니다.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DEFAULT_LOG_ROOT = PROJECT_ROOT / "logging"
DEFAULT_OUT_DIR = SCRIPT_DIR / "output"

MODULE_RANGE = range(0, 31)  # M0 .. M30
EXTRA_MODULES = (2, 3, 12, 19)
LOWER_BODY_MODULES = tuple(range(16, 31))  # M16 .. M30 (15개)


def find_latest_session_dir(log_root: Path) -> Path:
    if not log_root.is_dir():
        raise FileNotFoundError(f"logging 폴더가 없습니다: {log_root}")
    candidates = [p for p in log_root.iterdir() if p.is_dir()]
    if not candidates:
        raise FileNotFoundError(f"세션 하위 폴더가 없습니다: {log_root}")
    return max(candidates, key=lambda p: p.stat().st_mtime)


def read_module_csv(
    session_dir: Path, module_index: int, required_columns: tuple[str, ...] = ("nActualPosition",)
) -> pd.DataFrame | None:
    path = session_dir / f"M{module_index}.csv"
    if not path.is_file():
        return None
    df = pd.read_csv(path)
    for col in required_columns:
        if col not in df.columns:
            raise ValueError(f"{path} 에 '{col}' 컬럼이 없습니다. 컬럼: {list(df.columns)}")
    return df


def add_sample_index(df: pd.DataFrame) -> pd.DataFrame:
    """배치가 바뀌어도 꺾이지 않도록 파일 순서 기준 단조 x."""
    out = df.copy()
    out["_sample"] = np.arange(len(out), dtype=np.int64)
    return out


def plot_all_modules(session_dir: Path, out_path: Path, dpi: int) -> None:
    plt.figure(figsize=(14, 7))
    ax = plt.gca()
    turbo = mpl.colormaps["turbo"]
    n_mod = len(MODULE_RANGE)
    plotted = 0
    for i, m in enumerate(MODULE_RANGE):
        df = read_module_csv(session_dir, m)
        if df is None or df.empty:
            continue
        df = add_sample_index(df)
        ax.plot(
            df["_sample"],
            df["nActualPosition"],
            color=turbo(i / max(n_mod - 1, 1)),
            linewidth=0.9,
            alpha=0.85,
            label=f"M{m}",
        )
        plotted += 1
    if plotted == 0:
        plt.close()
        raise RuntimeError("M0~M30 CSV 중 읽을 수 있는 파일이 없습니다.")
    ax.set_xlabel("sample index (row order in CSV)")
    ax.set_ylabel("nActualPosition")
    ax.set_title(f"nActualPosition — all modules (M0–M30)\n{session_dir.name}")
    ax.grid(True, alpha=0.3)
    ax.legend(
        bbox_to_anchor=(1.02, 1),
        loc="upper left",
        fontsize=7,
        ncol=1,
        framealpha=0.9,
    )
    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=dpi, bbox_inches="tight")
    plt.close()
    print(f"저장: {out_path}")


def plot_all_modules_torque(session_dir: Path, out_path: Path, dpi: int) -> None:
    plt.figure(figsize=(14, 7))
    ax = plt.gca()
    turbo = mpl.colormaps["turbo"]
    n_mod = len(MODULE_RANGE)
    plotted = 0
    for i, m in enumerate(MODULE_RANGE):
        df = read_module_csv(session_dir, m, required_columns=("nActualTorque",))
        if df is None or df.empty:
            continue
        df = add_sample_index(df)
        ax.plot(
            df["_sample"],
            df["nActualTorque"],
            color=turbo(i / max(n_mod - 1, 1)),
            linewidth=0.9,
            alpha=0.85,
            label=f"M{m}",
        )
        plotted += 1
    if plotted == 0:
        plt.close()
        raise RuntimeError("M0~M30 CSV 중 읽을 수 있는 파일이 없습니다.")
    ax.set_xlabel("sample index (row order in CSV)")
    ax.set_ylabel("nActualTorque")
    ax.set_title(f"nActualTorque — all modules (M0–M30)\n{session_dir.name}")
    ax.grid(True, alpha=0.3)
    ax.legend(
        bbox_to_anchor=(1.02, 1),
        loc="upper left",
        fontsize=7,
        ncol=1,
        framealpha=0.9,
    )
    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=dpi, bbox_inches="tight")
    plt.close()
    print(f"저장: {out_path}")


def plot_single_module(session_dir: Path, module_index: int, out_path: Path, dpi: int) -> None:
    df = read_module_csv(session_dir, module_index)
    if df is None:
        print(f"건너뜀 (파일 없음): M{module_index}.csv", file=sys.stderr)
        return
    if df.empty:
        print(f"건너뜀 (데이터 없음): M{module_index}.csv", file=sys.stderr)
        return
    df = add_sample_index(df)

    fig, ax = plt.subplots(figsize=(12, 5))
    ax.plot(df["_sample"], df["nActualPosition"], color="C0", linewidth=1.2)
    ax.set_xlabel("sample index (row order in CSV)")
    ax.set_ylabel("nActualPosition")
    ax.set_title(f"M{module_index} — nActualPosition\n{session_dir.name}")
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=dpi, bbox_inches="tight")
    plt.close()
    print(f"저장: {out_path}")


def plot_torque_grid_3x5(session_dir: Path, out_path: Path, dpi: int) -> None:
    fig, axes = plt.subplots(3, 5, figsize=(18, 9), sharex=True)
    axes_flat = axes.flatten()
    plotted = 0

    for ax, m in zip(axes_flat, LOWER_BODY_MODULES):
        df = read_module_csv(session_dir, m, required_columns=("nActualTorque",))
        if df is None or df.empty:
            ax.set_title(f"M{m} (no data)")
            ax.grid(True, alpha=0.3)
            ax.text(
                0.5,
                0.5,
                "No data",
                ha="center",
                va="center",
                transform=ax.transAxes,
                fontsize=9,
                color="gray",
            )
            continue

        df = add_sample_index(df)
        ax.plot(df["_sample"], df["nActualTorque"], color="C1", linewidth=1.0)
        ax.set_title(f"M{m}")
        ax.grid(True, alpha=0.3)
        plotted += 1

    if plotted == 0:
        plt.close()
        raise RuntimeError("M16~M30 CSV 중 읽을 수 있는 토크 데이터가 없습니다.")

    for i, ax in enumerate(axes_flat):
        if i % 5 == 0:
            ax.set_ylabel("nActualTorque")
        if i >= 10:
            ax.set_xlabel("sample index")

    fig.suptitle(f"nActualTorque — M16–M30 (3x5)\n{session_dir.name}", fontsize=14)
    fig.tight_layout(rect=[0, 0.03, 1, 0.95])
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=dpi, bbox_inches="tight")
    plt.close(fig)
    print(f"저장: {out_path}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="logging 세션 CSV → nActualPosition/nActualTorque 그래프 (PNG)"
    )
    parser.add_argument(
        "--log-root",
        type=Path,
        default=DEFAULT_LOG_ROOT,
        help=f"logging 상위 폴더 (기본: {DEFAULT_LOG_ROOT})",
    )
    parser.add_argument(
        "--session",
        type=Path,
        default=None,
        help="특정 세션 폴더 경로 (미지정 시 log-root 아래 최근 폴더)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=DEFAULT_OUT_DIR,
        help=f"PNG 저장 폴더 (기본: {DEFAULT_OUT_DIR})",
    )
    parser.add_argument("--dpi", type=int, default=150, help="출력 해상도 (기본 150)")
    args = parser.parse_args()

    log_root = args.log_root.resolve()
    session_dir = args.session.resolve() if args.session else find_latest_session_dir(log_root)
    if not session_dir.is_dir():
        print(f"세션 폴더가 아닙니다: {session_dir}", file=sys.stderr)
        return 1

    out_dir = args.out_dir.resolve()
    prefix = session_dir.name

    try:
        plot_all_modules(
            session_dir,
            out_dir / f"{prefix}_all_M0-M30_nActualPosition.png",
            args.dpi,
        )
        plot_all_modules_torque(
            session_dir,
            out_dir / f"{prefix}_all_M0-M30_nActualTorque.png",
            args.dpi,
        )
        plot_torque_grid_3x5(
            session_dir,
            out_dir / f"{prefix}_M16-M30_nActualTorque_grid_3x5.png",
            args.dpi,
        )
        for m in EXTRA_MODULES:
            plot_single_module(
                session_dir,
                m,
                out_dir / f"{prefix}_M{m}_nActualPosition.png",
                args.dpi,
            )
    except (FileNotFoundError, ValueError, RuntimeError) as e:
        print(e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
