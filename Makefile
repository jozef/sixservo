all: README

clean:
	rm -f README README.bak

README: sixservo.ino
	pod2readme $< $@ && rm -f $@.bak
