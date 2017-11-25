#!/usr/bin/ruby
# (C) 2017 Shawn Landden, GPL-2+
#Create the databases (or cache) for command-not-found
#
#This program creates the database in /var/cache/command-not-found/db. It may
#take some time, because the files it reads are big.
#
#Patches are accepted, as well as rewrites.
#
#RE2 makes this twice as fast
require 're2'

paths = RE2::Regexp.new('^(?:(?:bin\/)|(?:sbin\/)|(?:usr\/bin\/)|(?:usr\/sbin\/)|(?:usr\/games\/))')
parse = RE2::Regexp.new('^[^ \t]*\/([^ \t]*)[ \t]*([^ \t]*)\/([^ \t]*)$')
db = []
`apt-get indextargets`.split("\n\n").grep(RE2("ShortDesc: Contents")).
map{|i| /Filename: ([^\n]*)\n/.match(i)[1]}.each { |repo|
	io = IO.popen(["/usr/lib/apt/apt-helper", "cat-file", repo])
	io.grep(paths) { |line|
		match = parse.match(line)
		if match[2].include?('/')
			component = match[2].split('/')[0]
	                db << "#{match[1]}\xff#{match[3].chomp}/#{component.byteslice(0,1)}"
		else
			component = "main"
	                db << "#{match[1]}\xff#{match[3].chomp}"
		end
	}
}
db.sort!
db.uniq!
out = open("/var/cache/command-not-found/db", "w+")
db.each { |i|
	out.write(i)
	out.putc("\n")
}
