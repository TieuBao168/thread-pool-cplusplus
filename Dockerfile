FROM ubuntu:24.10

# Install necessary build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a directory project
WORKDIR /app

# Copy your project files
COPY . .

# This will create the binary in the build folder
RUN make 2>&1 | tee build.log

# Set the working directory to the build folder
WORKDIR /app/build

# Set the entry point command
CMD ["sh", "-c", "./thread_pool_binary 2>&1 | tee run.log && cp run.log /app/"]
# Or use bash as entry point:
# CMD ["/bin/bash"]