sudo cp systemd/ambilight-led.service /etc/systemd/system/
sudo cp systemd/ambilight-api.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now ambilight-led.service
sudo systemctl enable --now ambilight-api.service
