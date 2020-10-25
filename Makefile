all: README.md

clean:
	rm -f README.md README.md.bak

README.md: sixservo.ino
	pod2readme --format markdown $< $@ && rm -f $@.bak
