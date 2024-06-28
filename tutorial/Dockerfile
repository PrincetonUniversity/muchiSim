# Use an official Ubuntu as a parent image
FROM ubuntu:latest

# Set the working directory inside the container
WORKDIR /root

# Install basic utilities
RUN apt-get update && \
    apt-get install -y \
    bash \
    g++ \
    python3 \
    python3-matplotlib \
    python3-pyqt5 \
    python3-imageio \
    && apt-get clean

# Ensure Bash starts when container is run
CMD ["bash"]
