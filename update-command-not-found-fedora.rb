#!/usr/bin/ruby

#100MB+ XML file these guys were smoking some serious stuff
# Takes 3.5 GB ram to parse at once.
#I'm just going to parse it as text
require 'nokogiri'
require 'open-uri'
require 'English'

db = []
package = ""

version="27"
arch= "x86_64"
base = "https://download.fedoraproject.org/pub/fedora/linux//updates/#{version}/#{arch}/"
fuckyeah = Nokogiri::XML(open("#{base}repodata/repomd.xml")).
	css("repomd data[type=filelists] location").attribute("href").value
zipped = open("#{base}#{fuckyeah}")
zlib  = Zlib::GzipReader.new zipped
zlib.each { |line|
	if /<package/ =~line then
		package = Regexp.new(' name="([^"]*)" ').match(line)[1]
	end
	if Regexp.new('^  <file>(?:(?:/usr/bin/)|(?:/usr/sbin/)|(?:/usr/games/))([^</]*)</file>$').match(line) then
		db << "#{$LAST_MATCH_INFO[1]}\xff#{package}"
	end
}
db.sort!
db.uniq!
out = open("/var/cache/command-not-found/db", "w+")
db.each { |i|
	out.write(i)
	out.putc("\n")
}
