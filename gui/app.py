import streamlit as st
import subprocess
import os
import tempfile
import io
import zipfile
import json
import datetime
from pathlib import Path

# Paths
BASE_DIR = Path(__file__).resolve().parent.parent
WITH_EPS_DIR = BASE_DIR / "WithEpsilon"
WITHOUT_EPS_DIR = BASE_DIR / "WithoutEpsilon"
OUTPUT_DIR = BASE_DIR / "output"
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

st.set_page_config(
    page_title="Petri Net Synthesis Studio",
    page_icon="🕸️",
    layout="wide"
)

st.title("🕸️ Petri Net Synthesis Studio")
st.caption("Synthesize Petri Nets from Traces with Loop Inference & Bounded Minimal Regions")

# ---------------------------------------------------------------------
# Helper Functions for .pns (Petri Net Synthesis) Bundles
# ---------------------------------------------------------------------

def render_dot_to_png(dot_code: str) -> bytes:
    """Render Graphviz DOT code to PNG binary data using the system 'dot' utility."""
    if not dot_code.strip():
        return b""
    try:
        proc = subprocess.run(
            ["dot", "-Tpng"],
            input=dot_code.encode("utf-8"),
            capture_output=True,
            check=True,
            timeout=15
        )
        return proc.stdout
    except Exception as e:
        st.warning(f"Note: Could not render PNG via 'dot': {e}")
        return b""

def create_pns_bundle(
    traces_text: str,
    pn_dot: str,
    ts_dot: str,
    pn_png: bytes,
    ts_png: bytes,
    metadata: dict
) -> bytes:
    """Create a .pns (ZIP) package containing traces, DOT files, PNG renders, and metadata."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("traces.txt", traces_text)
        zf.writestr("petri_net.dot", pn_dot)
        zf.writestr("transition_system.dot", ts_dot)
        if pn_png:
            zf.writestr("petri_net.png", pn_png)
        if ts_png:
            zf.writestr("transition_system.png", ts_png)
        zf.writestr("metadata.json", json.dumps(metadata, indent=2))
    return buf.getvalue()

def extract_traces_from_pns(pns_bytes: bytes) -> tuple[str, dict]:
    """Read a .pns file and extract the execution traces along with any metadata."""
    buf = io.BytesIO(pns_bytes)
    traces = ""
    metadata = {}
    try:
        with zipfile.ZipFile(buf, "r") as zf:
            if "traces.txt" in zf.namelist():
                traces = zf.read("traces.txt").decode("utf-8", errors="replace")
            if "metadata.json" in zf.namelist():
                try:
                    metadata = json.loads(zf.read("metadata.json").decode("utf-8", errors="replace"))
                except Exception:
                    pass
    except Exception as e:
        st.error(f"Failed to read .pns archive: {e}")
    return traces, metadata

# ---------------------------------------------------------------------
# Sidebar Configuration
# ---------------------------------------------------------------------
st.sidebar.header("⚙️ Configuration")

engine_choice = st.sidebar.radio(
    "Synthesis Engine:",
    options=["WithEpsilon (Testing / Enhanced)", "WithoutEpsilon (Baseline)"],
    index=0,
    help="Select which synthesis engine binary to execute."
)
is_with_epsilon_engine = "WithEpsilon" in engine_choice

kmax = st.sidebar.slider(
    "Max-k Region Weight Bound (kmax):",
    min_value=1,
    max_value=10,
    value=3,
    step=1,
    help="The maximum region weight / bound to search for minimal excitation-closed regions."
)

repeat_threshold = st.sidebar.number_input(
    "Loop Inference Repetition Threshold (x):",
    min_value=2,
    max_value=20,
    value=3,
    step=1,
    help="Consecutive occurrences of an event or pattern (e.g., b->b->b) to infer as a loop (e.g., b*)."
)

if is_with_epsilon_engine:
    allow_epsilon = st.sidebar.checkbox(
        "Allow Epsilon Transitions (ε-assisted)",
        value=True,
        help="Permit silent epsilon transitions to resolve state-separation conflicts and achieve bisimulation."
    )
else:
    allow_epsilon = False
    st.sidebar.info("ℹ️ Epsilon transitions disabled in WithoutEpsilon mode.")

# ---------------------------------------------------------------------
# Sample Traces & State Setup
# ---------------------------------------------------------------------
SAMPLE_TRACES = """a->b->b->b->d
a->c->c->c->d
a->b->b->b->b->c->c->c->d
a->c->c->c->b->b->b->b->d"""

if "traces_input" not in st.session_state:
    st.session_state["traces_input"] = SAMPLE_TRACES

def set_preset_traces(preset_str: str):
    """Callback to instantly update traces in session state."""
    st.session_state["traces_input"] = preset_str

def on_pns_file_uploaded():
    """Callback triggered whenever a user uploads a .pns file."""
    uploaded = st.session_state.get("pns_file_uploader")
    if uploaded is not None:
        traces, pns_meta = extract_traces_from_pns(uploaded.getvalue())
        if traces and traces.strip():
            st.session_state["traces_input"] = traces.strip()
            engine_info = f" (Originally created with {pns_meta.get('engine')}, kmax={pns_meta.get('kmax')})" if pns_meta.get("engine") else ""
            st.session_state["pns_notification"] = f"Loaded {len(traces.strip().splitlines())} trace(s) from `{uploaded.name}`!{engine_info}"
        else:
            st.session_state["pns_error_notification"] = f"No valid traces.txt found inside `{uploaded.name}`."

st.subheader("1. Input Execution Traces")
st.write(
    "Enter one trace per line. Events can be separated by `->` or spaces "
    f"(e.g., `a->b->b->b->d`). With repetition threshold = **{repeat_threshold}**, "
    f"3 consecutive `b`s will be inferred as `(b)*`."
)

col_input, col_presets = st.columns([3, 1])

with col_presets:
    st.markdown("**Quick Presets:**")
    st.button(
        "Preset: Branch & Loops",
        width="stretch",
        on_click=set_preset_traces,
        args=(SAMPLE_TRACES,)
    )
    st.button(
        "Preset: Simple Sequential",
        width="stretch",
        on_click=set_preset_traces,
        args=("a->b->c->d\na->b->d",)
    )
    st.button(
        "Preset: Pure Loops",
        width="stretch",
        on_click=set_preset_traces,
        args=("a->b->b->b->c\na->b->b->b->b->b->c",)
    )

    st.markdown("---")
    st.markdown("**📂 Load from .pns File:**")
    st.file_uploader(
        "Upload .pns file",
        type=["pns"],
        help="Load traces from a saved .pns project file into the editor.",
        key="pns_file_uploader",
        on_change=on_pns_file_uploaded,
        label_visibility="collapsed"
    )

with col_input:
    if "pns_notification" in st.session_state:
        st.success(st.session_state.pop("pns_notification"), icon="📂")
    if "pns_error_notification" in st.session_state:
        st.error(st.session_state.pop("pns_error_notification"), icon="⚠️")

    traces_text = st.text_area(
        "Traces (one per line):",
        height=210,
        placeholder="a->b->c->d\na->b->b->b->d",
        key="traces_input"
    )

# Execution Button
run_synthesis = st.button("🚀 Synthesize Petri Net", type="primary", width="stretch")

if run_synthesis:
    clean_traces = [line.strip() for line in traces_text.strip().splitlines() if line.strip() and not line.strip().startswith("#")]
    
    if not clean_traces:
        st.error("Please provide at least one valid trace!")
    else:
        engine_dir = WITH_EPS_DIR if is_with_epsilon_engine else WITHOUT_EPS_DIR
        exe_path = engine_dir / "run.exe"

        if not exe_path.exists():
            st.error(f"Binary not found at `{exe_path}`. Please compile it first.")
        else:
            with st.spinner("Running Petri Net Synthesis..."):
                with tempfile.TemporaryDirectory() as tmpdir:
                    tmpdir_path = Path(tmpdir)
                    trace_file = tmpdir_path / "traces.txt"
                    ts_dot_file = tmpdir_path / "transition_system.dot"
                    pn_dot_file = tmpdir_path / "petri_net.dot"
                    report_file = tmpdir_path / "report.md"

                    # Write traces to temporary file
                    with open(trace_file, "w") as f:
                        for tr in clean_traces:
                            f.write(tr + "\n")

                    # Construct command
                    cmd = [
                        str(exe_path),
                        "--trace-file", str(trace_file),
                        "--kmax", str(kmax),
                        "--repeat-threshold", str(repeat_threshold),
                        "--out-pn-dot", str(pn_dot_file),
                        "--out-ts-dot", str(ts_dot_file),
                        "--out-report", str(report_file)
                    ]

                    if is_with_epsilon_engine:
                        cmd.extend(["--epsilon", "1" if allow_epsilon else "0"])

                    try:
                        proc = subprocess.run(
                            cmd,
                            cwd=str(engine_dir),
                            capture_output=True,
                            text=True,
                            timeout=60
                        )

                        if proc.returncode != 0:
                            st.error(f"Synthesis failed with exit code {proc.returncode}")
                            st.code(proc.stderr or proc.stdout, language="bash")
                        else:
                            stdout_text = proc.stdout or ""
                            report_text = report_file.read_text() if report_file.exists() else ""
                            ts_dot_code = ts_dot_file.read_text() if ts_dot_file.exists() else ""
                            pn_dot_code = pn_dot_file.read_text() if pn_dot_file.exists() else ""

                            # Determine synthesis outcome: bisimilar synthesis vs mining-only fallback
                            is_mined = (
                                "SYNTHESIS_STATUS: NOT_POSSIBLE_MINED" in stdout_text
                                or "Synthesis Not Possible, Petri net is mined" in stdout_text
                                or "mining-only" in stdout_text
                                or "bisimilar NOT reached by kmax" in stdout_text
                                or "NO (mining-only overapproximation)" in report_text
                            )
                            is_eps_assisted = (
                                "SUCCESS_BISIMILAR_WITH_EPSILON" in stdout_text
                                or "WITH epsilon assistance" in stdout_text
                                or "via epsilon transitions" in stdout_text
                            )

                            # Generate PNG renderings of both models
                            pn_png = render_dot_to_png(pn_dot_code) if pn_dot_code else b""
                            ts_png = render_dot_to_png(ts_dot_code) if ts_dot_code else b""

                            # Package metadata and .pns bundle
                            meta = {
                                "engine": "WithEpsilon" if is_with_epsilon_engine else "WithoutEpsilon",
                                "kmax": kmax,
                                "repeat_threshold": repeat_threshold,
                                "allow_epsilon": allow_epsilon if is_with_epsilon_engine else False,
                                "is_mined": is_mined,
                                "is_eps_assisted": is_eps_assisted,
                                "timestamp": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                            }

                            pns_bytes = create_pns_bundle(
                                traces_text=traces_text,
                                pn_dot=pn_dot_code,
                                ts_dot=ts_dot_code,
                                pn_png=pn_png,
                                ts_png=ts_png,
                                metadata=meta
                            )

                            # Also save a copy to the local output/ folder for persistence
                            local_pns_file = OUTPUT_DIR / "latest_synthesis.pns"
                            try:
                                local_pns_file.write_bytes(pns_bytes)
                                if pn_png:
                                    (OUTPUT_DIR / "petri_net.png").write_bytes(pn_png)
                                if ts_png:
                                    (OUTPUT_DIR / "transition_system.png").write_bytes(ts_png)
                            except Exception:
                                pass

                            # Save to session_state so results persist across button clicks / reruns
                            st.session_state["last_synthesis"] = {
                                "ts_dot_code": ts_dot_code,
                                "pn_dot_code": pn_dot_code,
                                "pn_png": pn_png,
                                "ts_png": ts_png,
                                "pns_bytes": pns_bytes,
                                "report_content": report_text,
                                "stdout_text": stdout_text,
                                "is_mined": is_mined,
                                "is_eps_assisted": is_eps_assisted,
                                "traces_text": traces_text,
                                "timestamp": datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
                            }

                    except subprocess.TimeoutExpired:
                        st.error("Synthesis timed out after 60 seconds.")
                    except Exception as e:
                        st.error(f"Execution error: {str(e)}")

# ---------------------------------------------------------------------
# Display Results (from current run or persisted in session state)
# ---------------------------------------------------------------------
if "last_synthesis" in st.session_state:
    res = st.session_state["last_synthesis"]
    is_mined = res["is_mined"]
    is_eps_assisted = res["is_eps_assisted"]

    st.markdown("---")
    if is_mined:
        st.warning("⚠️ Synthesis Not Possible, Petri net is mined")
    elif is_eps_assisted:
        st.success("🎉 Successfully synthesized Petri net (bisimilar achieved with ε-transitions)!")
    else:
        st.success("🎉 Successfully synthesized Petri net (bisimilar)!")

    # Side-by-side Graph Visualization
    st.subheader("2. Visual Models")
    col1, col2 = st.columns(2)

    with col1:
        st.markdown("### 🔄 Transition System")
        if res["ts_dot_code"]:
            st.graphviz_chart(res["ts_dot_code"], width="stretch")
            with st.expander("View Transition System DOT Code"):
                st.code(res["ts_dot_code"], language="dot")
        else:
            st.warning("Transition System DOT was not generated.")

    with col2:
        if is_mined:
            st.markdown("### 🕸️ Mined Petri Net (Overapproximation)")
        else:
            st.markdown("### 🕸️ Synthesized Petri Net (Bisimilar)")
        if res["pn_dot_code"]:
            st.graphviz_chart(res["pn_dot_code"], width="stretch")
            with st.expander("View Petri Net DOT Code"):
                st.code(res["pn_dot_code"], language="dot")
        else:
            st.warning("Petri Net DOT was not generated.")

    # -----------------------------------------------------------------
    # Save & Export Section (.pns bundle download)
    # -----------------------------------------------------------------
    st.subheader("💾 Save & Export (.pns)")
    st.write(
        "Save complete synthesis results into a **`.pns`** file containing the **Traces**, "
        "generated **Petri Net** & **Transition System** (DOT code), and their high-resolution **PNG images**."
    )

    tstamp = res.get("timestamp", datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))

    st.download_button(
        label="💾 Save Project (.pns)",
        data=res["pns_bytes"],
        file_name=f"petrinet_model_{tstamp}.pns",
        mime="application/octet-stream",
        help="Download .pns package containing Traces, Petri Net & Transition System (DOT + PNG images).",
        width="stretch"
    )

    # Report Section
    st.subheader("3. Synthesis Details & Report")
    if res.get("report_content"):
        st.markdown(res["report_content"])
    elif res.get("stdout_text"):
        st.info("CLI Output:")
        st.text(res["stdout_text"])
