#HOW TO RUN

##Precondition
###Get node modules using package.json
```sh
npm install
```

###Run Chrome on Browserside
```sh
google-chrome  --remote-debugging-port=9222 --aggressive-cache-discard --disable-application-cache
```

###Run servercode
Run renderer-side server.py (Plz refer to README.md in ../renderer)

##Run Client Auto test code
in client side : 
```sh
python ./runAutomationTool.py [server_ip] [server_port]
```

