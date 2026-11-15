# Appendix: Task F — Backpropagation Derivation

This appendix contains the formal mathematical derivation and step-by-step numerical calculation for Task F: Simple Backpropagation.

---

## Page 1: Problem Definition, Forward Pass & Chain Rule Derivations

### 1. Model Architecture & Parameters
- **Input Sample:** $x = 1.0$
- **Target Value:** $y = 2.0$
- **Forward Pass Equations:**
  $$z = w x + b$$
  $$h = \tanh(z)$$
  $$\hat{y} = v h + c$$
  $$J = \frac{1}{2} (\hat{y} - y)^2$$
- **Initial Parameter Values:**
  $$w = 0.5, \quad b = 0.0, \quad v = 1.0, \quad c = 0.0$$
  $$\text{Learning Rate: } \eta = 0.1$$

---

### 2. Forward Pass Evaluation (Step 0)
1. **Pre-activation:**
   $$z = w \cdot x + b = (0.5)(1.0) + 0.0 = \mathbf{0.50000000}$$

2. **Hidden Activation:**
   $$h = \tanh(z) = \tanh(0.5) = \frac{e^{0.5} - e^{-0.5}}{e^{0.5} + e^{-0.5}} \approx \mathbf{0.46211716}$$

3. **Output Prediction:**
   $$\hat{y} = v \cdot h + c = (1.0)(0.46211716) + 0.0 = \mathbf{0.46211716}$$

4. **Residual Error ($e$):**
   $$e = \hat{y} - y = 0.46211716 - 2.00000000 = \mathbf{-1.53788284}$$

5. **Initial Objective Loss ($J$):**
   $$J = \frac{1}{2} (\hat{y} - y)^2 = \frac{1}{2} (-1.53788284)^2 = \frac{1}{2}(2.36508363) = \mathbf{1.18254182}$$

---

### 3. Analytical Chain-Rule Gradient Derivations
Using the chain rule of differential calculus:

#### A. Gradient with respect to Output Bias $c$:
$$\frac{\partial J}{\partial c} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial c} = (\hat{y} - y) \cdot (1) = e$$
$$\mathbf{\frac{\partial J}{\partial c} = -1.53788284}$$

#### B. Gradient with respect to Output Weight $v$:
$$\frac{\partial J}{\partial v} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial v} = (\hat{y} - y) \cdot h = e \cdot h$$
$$\frac{\partial J}{\partial v} = (-1.53788284) \times (0.46211716) = \mathbf{-0.71068205}$$

#### C. Backpropagation to Hidden Layer $h$ and $z$:
$$\frac{\partial J}{\partial h} = \frac{\partial J}{\partial \hat{y}} \cdot \frac{\partial \hat{y}}{\partial h} = (\hat{y} - y) \cdot v = e \cdot v = (-1.53788284)(1.0) = -1.53788284$$

Derivative of hyperbolic tangent activation:
$$\frac{dh}{dz} = \frac{d}{dz}[\tanh(z)] = 1 - \tanh^2(z) = 1 - h^2$$
$$1 - h^2 = 1 - (0.46211716)^2 = 1 - 0.21355227 = \mathbf{0.78644773}$$

By chain rule for pre-activation $z$:
$$\frac{\partial J}{\partial z} = \frac{\partial J}{\partial h} \cdot \frac{dh}{dz} = [(\hat{y} - y) \cdot v] \cdot (1 - h^2)$$
$$\frac{\partial J}{\partial z} = (-1.53788284) \times (0.78644773) = \mathbf{-1.20946448}$$

#### D. Gradient with respect to Hidden Bias $b$:
$$\frac{\partial J}{\partial b} = \frac{\partial J}{\partial z} \cdot \frac{\partial z}{\partial b} = \frac{\partial J}{\partial z} \cdot (1)$$
$$\mathbf{\frac{\partial J}{\partial b} = -1.20946448}$$

#### E. Gradient with respect to Input Weight $w$:
$$\frac{\partial J}{\partial w} = \frac{\partial J}{\partial z} \cdot \frac{\partial z}{\partial w} = \frac{\partial J}{\partial z} \cdot x$$
Since $x = 1.0$:
$$\mathbf{\frac{\partial J}{\partial w} = -1.20946448 \times 1.0 = -1.20946448}$$

---

## Page 2: Gradient Check, Parameter Update & Gradient Direction Analysis

### 4. Central Finite Difference Verification ($\epsilon = 10^{-6}$)
Formula for central finite difference numerical approximation:
$$\frac{\partial J}{\partial \theta} \approx \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}, \quad \epsilon = 10^{-6}$$

| Parameter ($\theta$) | Analytical Gradient | Numerical Gradient (FD) | Absolute Error | Tolerance ($\le 10^{-5}$) | Result |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **$w$** | -1.20946448 | -1.20946448 | $1.13 \times 10^{-10}$ | $1.0 \times 10^{-5}$ | **PASSED** |
| **$b$** | -1.20946448 | -1.20946448 | $1.13 \times 10^{-10}$ | $1.0 \times 10^{-5}$ | **PASSED** |
| **$v$** | -0.71068205 | -0.71068205 | $5.25 \times 10^{-11}$ | $1.0 \times 10^{-5}$ | **PASSED** |
| **$c$** | -1.53788284 | -1.53788284 | $1.12 \times 10^{-10}$ | $1.0 \times 10^{-5}$ | **PASSED** |

*All analytical gradients match central finite differences to within $\approx 10^{-10}$, exceeding the required tolerance by five orders of magnitude.*

---

### 5. Simultaneous Gradient Descent Parameter Update
Using the learning rate $\eta = 0.1$ and gradients calculated from the original Step 0 parameters:
$$\theta^{(1)} = \theta^{(0)} - \eta \frac{\partial J}{\partial \theta}$$

1. **Update for $w$:**
   $$w^{(1)} = 0.50000000 - (0.1) \cdot (-1.20946448) = 0.50000000 + 0.12094645 = \mathbf{0.62094645}$$

2. **Update for $b$:**
   $$b^{(1)} = 0.00000000 - (0.1) \cdot (-1.20946448) = 0.00000000 + 0.12094645 = \mathbf{0.12094645}$$

3. **Update for $v$:**
   $$v^{(1)} = 1.00000000 - (0.1) \cdot (-0.71068205) = 1.00000000 + 0.07106821 = \mathbf{1.07106820}$$

4. **Update for $c$:**
   $$c^{(1)} = 0.00000000 - (0.1) \cdot (-1.53788284) = 0.00000000 + 0.15378828 = \mathbf{0.15378828}$$

---

### 6. Engineering Explanation of Gradient Direction
1. **Physical Meaning of Negative Gradients:**
   At initialization, the prediction is $\hat{y} \approx 0.462$, whereas the target is $y = 2.0$. The model severely underpredicts the target, giving a negative residual:
   $$e = \hat{y} - y = -1.538 < 0$$
2. **Monotonic Influence of Weights:**
   Since $v = 1.0 > 0$ and $h = \tanh(0.5) > 0$:
   - Increasing output bias $c$ directly increases $\hat{y}$ ($\frac{\partial \hat{y}}{\partial c} = 1 > 0$).
   - Increasing output weight $v$ directly increases $\hat{y}$ ($\frac{\partial \hat{y}}{\partial v} = h > 0$).
   - Increasing hidden weight $w$ and bias $b$ increases $z$, which strictly increases $h$ (since $\frac{dh}{dz} = 1 - h^2 > 0$), which in turn increases $\hat{y}$.
3. **Descent Direction ($-\nabla J$):**
   Because increasing *any* parameter increases $\hat{y}$ towards the target $2.0$ (thereby reducing the squared error $J$), the partial derivative $\frac{\partial J}{\partial \theta}$ must be negative for all four parameters.
   Under standard gradient descent:
   $$\Delta \theta = -\eta \frac{\partial J}{\partial \theta} > 0$$
   Therefore, the algorithm steps in the positive direction for all four variables, steadily driving $\hat{y} \to 2.0$ and loss $J \to 0$.

