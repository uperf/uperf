FILE=uperf
CMD=xsltproc --xinclude --output  $(FILE).html $(FILE).xsl $(FILE).xml

all: $(FILE).xml $(FILE).xsl
	time /usr/bin/xsltproc --xinclude -o $(FILE).html $(FILE).xsl $(FILE).xml
