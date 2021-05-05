package main

import (
	"encoding/binary"
	"flag"
	"log"
	"math"
	"net/http"
	"time"

	"github.com/gorilla/websocket"
)

var addr = flag.String("addr", "localhost:8080", "http service address")

var upgrader = websocket.Upgrader{} // use default options

func syncecho(w http.ResponseWriter, r *http.Request) {
	log.Println(r)
	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Print("upgrade:", err)
		return
	}
	defer c.Close()
	for {
		mt, message, err := c.ReadMessage()
		if err != nil {
			log.Println("read:", err)
			break
		}

		switch mt {
		case websocket.TextMessage:
			log.Println(string(message))
		case websocket.BinaryMessage:
			err = c.WriteMessage(mt, message)
			if err != nil {
				log.Println("write:", err)
				break
			}
		default:

		}
	}
}

func Sln16SynGen(samplerate float64, frequency float64) func(n int) []byte {
	mult := math.Pow(2, 16) / 4
	pos := 0

	return func(n int) []byte {
		buf := make([]byte, 0, n*2)
		b := make([]byte, 2)
		for i := 0 + pos; i < n+pos; i++ {
			x := math.Pi * 2 / (samplerate / frequency)
			s := math.Sin(x*float64(i)) * mult
			binary.LittleEndian.PutUint16(b, uint16(s))
			buf = append(buf, b...)
		}
		pos += n
		return buf
	}
}

func asyncsin(w http.ResponseWriter, r *http.Request) {
	log.Println(r)
	if c, err := upgrader.Upgrade(w, r, nil); err != nil {
		log.Print("Upgrade:", err)
		return
	} else {
		defer c.Close()
		go func() {
			for {
				if mt, message, err := c.ReadMessage(); err != nil {
					log.Println("ReadMessage:", err)
					break
				} else {
					switch mt {
					case websocket.TextMessage:
						log.Println(string(message))
					case websocket.BinaryMessage:

					default:

					}

				}
			}
		}()

		nextframe := Sln16SynGen(8000, 1000)

		ticker := time.NewTicker(21 * time.Millisecond)
		for {
			<-ticker.C
			if err = c.WriteMessage(websocket.BinaryMessage, nextframe(160)); err != nil {
				log.Println("WriteMessage:", err)
				break
			}

		}
		ticker.Stop()

	}
}

func syncsin(w http.ResponseWriter, r *http.Request) {
	log.Println(r)
	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Print("upgrade:", err)
		return
	}
	defer c.Close()
	nextframe := Sln16SynGen(8000, 1000)
	for {
		mt, message, err := c.ReadMessage()
		if err != nil {
			log.Println("read:", err)
			break
		}

		switch mt {
		case websocket.TextMessage:
			log.Println(string(message))
		case websocket.BinaryMessage:

			err = c.WriteMessage(mt, nextframe(160))
			if err != nil {
				log.Println("write:", err)
				break
			}
		default:

		}
	}
}

func main() {
	flag.Parse()
	log.SetFlags(0)
	http.HandleFunc("/syncecho", syncecho)
	http.HandleFunc("/syncsin", syncsin)
	http.HandleFunc("/asyncsin", asyncsin)
	log.Println("start listening at", *addr)
	log.Fatal(http.ListenAndServe(*addr, nil))
}
