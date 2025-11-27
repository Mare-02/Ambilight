sudo apt update
sudo apt install -y build-essential cmake git python3 python3-venv python3-dev
# SPI
sudo raspi-config nonint do_spi 0  # oder manuell über raspi-config
