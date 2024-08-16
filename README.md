# webserver in C
A web server written in C. Serves HTML, JS, CSS, PNG, JPG, ICO files, the server uses fork sys calls to handle multiple incoming client requests.
The server file is just the c file, which can be compiled and run with port no given in second augument.
Then the example files can be accessed via server. index.html file can be fetched through localhost:[portno] or localhost[portno]/index.html.

The server code is highly inspired by Dr. Jonas Birch video: https://www.youtube.com/watch?v=mXoWlrzb1Ok&pp=ygUPZHIuIGpvbmFzIGJpcmNo , With few more improvements done from my side.

![webserver](https://github.com/user-attachments/assets/55dac27c-16f7-4ac9-a09c-f626b06c12d2)
