# Use the official Ubuntu 22.04 image
FROM ubuntu:22.04

# Evita prompts durante a instalação
ENV DEBIAN_FRONTEND=noninteractive

# Atualiza os repositórios e instala as dependências necessárias:
# - openjdk-17-jdk (versão padrão disponível no 22.04)
# - ant
# - build-essential
# - locales (para suportar UTF-8)
RUN apt-get update && \
    apt-get install -y \
    ca-certificates \
    build-essential \
    nasm \
    binutils \
    git \
    locales \
    curl \
    && locale-gen pt_BR.UTF-8 && \
    update-locale LANG=pt_BR.UTF-8 LC_ALL=pt_BR.UTF-8 && \
    apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Instalar GitHub CLI (gh)
RUN mkdir -p -m 755 /etc/apt/keyrings && \
    curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg -o /etc/apt/keyrings/githubcli-archive-keyring.gpg && \
    chmod a+r /etc/apt/keyrings/githubcli-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" > /etc/apt/sources.list.d/github-cli.list && \
    apt-get update && \
    apt-get install -y gh && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Locale em UTF-8
ENV LANG=pt_BR.UTF-8
ENV LC_ALL=pt_BR.UTF-8

# Instalar NVM (Node Version Manager)
ENV NVM_DIR=/root/.nvm
RUN curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash && \
    . $NVM_DIR/nvm.sh && \
    nvm install 22.21.1

# Adicionar nvm ao PATH
ENV PATH=$NVM_DIR/versions/node/v22.21.1/bin:$PATH

# Instalar GitHub Copilot CLI globalmente
RUN npm install -g @github/copilot

# Instalar o marketplace e o plugin do Superpowers
RUN copilot plugin marketplace add obra/superpowers-marketplace && \
    copilot plugin install superpowers@superpowers-marketplace

# Define o comando padrão para abrir um shell
CMD ["tail", "-f", "/dev/null"]
