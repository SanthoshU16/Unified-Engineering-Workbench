# ========================================================
# Stage 1: Build the C++ executable
# ========================================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy CMake build definition and backend source tree
COPY CMakeLists.txt ./
COPY backend/ ./backend/

# Build optimized Release binary
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -j$(nproc)

# ========================================================
# Stage 2: Minimal runtime image
# ========================================================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled binary from builder stage
COPY --from=builder /app/build/backend/workbench_server ./workbench_server

# Copy static frontend assets, sample datasets, results, and documentation
COPY frontend/ ./frontend/
COPY samples/ ./samples/
COPY results/ ./results/
COPY docs/ ./docs/

# Default port for Render Web Services
ENV PORT=10000
EXPOSE 10000

# Execute server without opening desktop browser
CMD ["./workbench_server", "--no-browser", "--web-root", "."]
