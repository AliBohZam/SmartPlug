"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
Copyright (c) 2020-2021 Ali Bohloolizamani

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
import time
import requests
import matplotlib.pyplot as plt

if __name__ == '__main__':
    fig, ax = plt.subplots(nrows=2, ncols=1);
    fig.suptitle('SmartPlug 2021', fontsize=16);
    plt.ion();
    try:
        while (1):
            currentData = requests.get('http://************SP.local/current.log'); # put your ESP8266's mac adrs
            if (len(currentData.text) == 0):
                continue;
            else:
                currentResults = [int("{:02x}".format(ord(c)), 16) for c in currentData.text];
                current = [];
                power = [];
                temp = 0;
                for i in range(0, len(currentResults)):
                    if (i % 2 == 0):
                        temp = currentResults[i];
                    else:
                        current.append((currentResults[i] << 8) + temp);
                        power.append(220.0 * ((currentResults[i] << 8) + temp) / 1000.0)
                ax[0].set_ylabel("Current mA");
                ax[0].set_xlabel("Time 1Sec");
                ax[0].autoscale_view(True, True, True);
                t = range(0, len(current));
                ax[0].plot(t, current);
                ax[1].set_ylabel("Power Watt");
                ax[1].set_xlabel("Time 1Sec");
                ax[1].autoscale_view(True, True, True);
                t = range(0, len(power));
                ax[1].plot(t, power);
                plt.draw();
                plt.pause(100);
                ax[0].clear();
                ax[1].clear();
    except Exception as ex:
        print(ex);
        print("\ndone!");
