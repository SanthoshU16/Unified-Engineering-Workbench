# Artificial Intelligence Assistance & Evaluation Log

**PASS Science Works | R&D Internship Assessment 2026–2027**  
**Document ID:** UEW-AI-LOG-001  
**Assessment Criterion:** Prompt Engineering & AI Judgement (8 Points)  

---

## 1. Disclosure of AI Assistance & Tools

In compliance with Section 1 and Section 8 of the assessment brief:
- **Primary AI Model**: Google DeepMind Gemini 3.8 Flash (High Reasoning mode).
- **Environment**: Antigravity Pair-Programming Agentic Workbench.
- **Scope of AI Use**: Code scaffolding, mathematical verification checks, test case generation, and cross-platform CMake formulation. All derivations, numerical methods, and software architectures were independently reviewed, validated, and debugged by the candidate.

---

## 2. Representative Interactions & Prompt Log

### Interaction 1: 1D Thermal FEA Formulation & Assembly
- **Candidate Prompt:**  
  *"Derive the element stiffness matrix and load vector for the 1D steady-state heat equation -d/dx(kA dT/dx) = qA using two-node linear elements. Ensure the linear system assembly preserves strict symmetry and solves in O(N) operations."*
- **Model Output Summary:**  
  Provided Galerkin weak form derivation, showing $K^e = \frac{kA}{h} \begin{bmatrix} 1 & -1 \\ -1 & 1 \end{bmatrix}$ and $F^e = \frac{qAh}{2} \begin{bmatrix} 1 \\ 1 \end{bmatrix}$. Suggested Gaussian elimination for the global $(N+1) \times (N+1)$ matrix.
- **Candidate Edits & Refinements:**  
  Replaced Gaussian elimination with the **Tridiagonal Matrix Algorithm (Thomas algorithm / TDMA)**, reducing computational complexity from $O(N^3)$ to optimal $O(N)$ and eliminating unnecessary heap allocations.

---

### Interaction 2: Analytical Backpropagation Chain Rule
- **Candidate Prompt:**  
  *"For model h = tanh(wx + b), y_hat = vh + c, loss J = 0.5(y_hat - y)^2, derive analytical partial derivatives w.r.t w, b, v, c and compute step 0 values for x=1, y=2, w=0.5, b=0, v=1, c=0."*
- **Model Output Summary:**  
  Calculated:
  - $z = 0.5, h = \tanh(0.5) \approx 0.46211716, \hat{y} \approx 0.46211716, J \approx 1.18254182$.
  - Gradients: $\frac{\partial J}{\partial c} \approx -1.53788284, \frac{\partial J}{\partial v} \approx -0.71068205, \frac{\partial J}{\partial b} \approx -1.20946448, \frac{\partial J}{\partial w} \approx -1.20946448$.
- **Candidate Independent Verification:**  
  Wrote a Python verification script evaluating numerical central finite differences with $\epsilon = 10^{-6}$. Verified that analytical gradients match finite differences with absolute error $< 1.13 \times 10^{-10} \le 10^{-5}$, fully validating the chain rule formulation.

---

## 3. Deliberate Challenge Test & Incorrect Suggestion Identified

### Issue Identified: The "Nodal Superconvergence" Flaw
- **The AI Suggestion:**  
  When asked how to demonstrate mesh convergence for the thermal problem, the AI initially suggested:  
  *"Compute the maximum nodal error by comparing the discrete finite element nodal temperatures $T_i$ against $T_{\text{exact}}(x_i)$ for $N = 10, 20, 40, 80$."*

- **Why This Suggestion Was Unsuitable / Defective:**  
  For 1D Poisson problems with linear elements and uniform load, the finite element solution exhibits **superconvergence at the nodes** because the Green's function lies within the linear finite element space. As a result, $T_{\text{FE}}(x_i) \equiv T_{\text{exact}}(x_i)$ at all mesh nodes up to machine floating-point precision ($\approx 10^{-14\ \circ}\text{C}$):
  - At $N=10$, max nodal error was $2.84 \times 10^{-14\ \circ}\text{C}$.
  - At $N=80$, max nodal error was $1.22 \times 10^{-12\ \circ}\text{C}$ (actually slightly larger due to floating-point roundoff accumulation!).
  Following the AI's naive advice would lead to the absurd conclusion that $N=10$ has higher accuracy than $N=80$, completely failing to exhibit the theoretical $O(h^2)$ mesh convergence curve required by Task C!

- **Candidate Correction & Engineering Action:**  
  The candidate identified this mechanical-engineering nuance and instructed the solver to evaluate errors at **element midpoints**:
  $$x_{\text{mid}}^e = \frac{x_i + x_{i+1}}{2}, \quad T_{\text{FE}}(x_{\text{mid}}^e) = \frac{T_i + T_{i+1}}{2}$$
  Because $T_{\text{exact}}(x)$ is quadratic while the FE solution is piecewise linear, the midpoint error is:
  $$E_{\text{mid}} = \frac{1}{8} \left| \frac{d^2T}{dx^2} \right| h^2 = \frac{q}{8k} h^2 = 125 h^2$$
  Evaluating at element midpoints yielded the true physical convergence rates:
  - $N=10$ ($h=0.1$): Error = $1.25000000^\circ\text{C}$
  - $N=20$ ($h=0.05$): Error = $0.31250000^\circ\text{C}$ ($\text{Ratio} = 4.00$, $\alpha = 2.000$)
  - $N=40$ ($h=0.025$): Error = $0.07812500^\circ\text{C}$ ($\text{Ratio} = 4.00$, $\alpha = 2.000$)
  - $N=80$ ($h=0.0125$): Error = $0.01953125^\circ\text{C}$ ($\text{Ratio} = 4.00$, $\alpha = 2.000$)
  This deliberate identification and correction proved critical to achieving an exact $O(h^2)$ second-order convergence curve.

---

## 4. Later Product Refinement Assistance

On 2026-11-11, OpenAI Codex assisted with a later refinement pass requested by the candidate. Changes included the warm graphite-and-copper interface redesign, responsive navigation and status presentation, JSON escaped-string decoding for local file uploads, shared CPU model/core detection, benchmark metadata and report synchronization, and packaging-status disclosures. The candidate should review these changes, verify them in the target Windows environment, and update this disclosure if any implementation details change before submission.

---

### Issue Identified: Thread-Pool Lock Contention Under Fine-Grained Workloads
- **The AI Suggestion:**  
  The AI initially suggested dispatching each thermal case independently to the thread pool.
- **Candidate Correction:**  
  The implementation uses **chunked batch dispatch**, assigning contiguous ranges to workers to keep queue synchronization bounded and preserve deterministic result ordering. The current benchmark report records measured efficiency for its stated machine, workload, and run rather than treating a timing as a portable performance guarantee.

## 5. Submission Audit & Final UI Theme Pass (2026-11-15)

- **Tool / model:** OpenAI Codex, GPT-6, used in the Codex desktop workspace.
- **Representative candidate prompts:** The candidate asked for a distinctive, market-quality engineering-workbench UI palette and then asked Codex to compare the complete application with `Assignment_Only_Phase_version.pdf` for submission readiness.
- **Model output and edits:** Codex changed the shared interface palette to midnight plum and ion orchid; aligned the thermal chart/canvas surfaces and hardware cards with that palette; retained the temperature heatmap colors; and corrected the misspelled top-level CMake minimum-version command after the source build failed to configure.
- **Validation evidence:** On macOS / Apple silicon, the corrected source configured and built with CMake, CTest passed (14/14 backend checks), and `node --check` passed for the frontend JavaScript files. Windows packaging and clean-machine launch remain unverified.
- **Remaining review:** The candidate should review the changed source and this log. No handwritten work, Windows runtime, or timestamped demonstration evidence was generated during this audit.


## 6. Final Assignment Audit & Folder Alignment (2026-11-15)

- **Tool / model:** OpenAI Codex (GPT-6) in the Codex desktop workspace.
- **Candidate prompt:** The candidate asked Codex to compare the project against the supplied `Assignment_Only_Phase_version.pdf`, check every requirement, and keep the project folders and docs current for submission.
- **Relevant findings:** The assignment explicitly requires the `src/`, `data/`, and `tests/` folders; a Windows x64 `Launch.exe` and packaged server; a report of at most six pages; and a 3–5 minute video or timestamped screenshot walkthrough with logs. The existing screenshots were static and the Windows runtime was absent. The report and walkthrough also contained stale benchmark figures.
- **Edits prepared with AI assistance:** Aligned the repository layout to `src/`, `data/`, and `tests/`; updated CMake, launchers, Docker paths, sample paths, editor configuration, and packaging checks; added a native Windows launcher source and PowerShell runtime build helper; synchronized the benchmark report with the checked-in CSV; and added a six-page PDF report and renderer, dependency record, submission audit, and walkthrough notes.
- **Validation evidence:** On macOS / Apple silicon, `./scripts/build.sh` completed and CTest passed 14/14 checks. Node.js 22.19.0 syntax checks passed for all frontend JavaScript files. The local HTTP listener was confirmed on `127.0.0.1:18080`; page, stylesheet, and module returned 200, the sample STL parsed as 72 triangles, and the synthetic CSV parsed as 1,440 nodes. Windows compilation/launch and timestamped demo evidence remain outstanding.
- **Candidate review required:** Review these edits and run `scripts/build_windows_runtime.ps1` on Windows 11 x64 before packaging. No Windows runtime, capture timestamps, or video were fabricated during this audit.
