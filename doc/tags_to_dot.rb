#!/usr/bin/ruby
#
# 	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
#
#	This file is part of simavr.
#
#	simavr is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	simavr is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with simavr.  If not, see <http://www.gnu.org/licenses/>.

$files = Hash.new
$syms = Hash.new

tags = File.new(ARGV[0])

key = Array.new

while !tags.eof?
	next unless tags.readline.chomp.match(/([^\t]+)\t([^\t]+)\t(.*)\$\/;"\t([^\t]+)/);
	key[0] = $1;
	key[1] = $2;
	key[3] = $3;
	key[4] = $4;

	next if key[4] == 'm' or key[4] == 't'  or key[4] == 's'  or key[4] == 'e'  or key[4] == 'v';
	next if key[0].match(/[us]?int[0-9]+_t/);
	next if key[0] == "ROM_BASED";

	key[1].gsub!(/.*\/|\.[ch]$/,"");

	unless $files.key? key[1]
		$files[key[1]] = Hash.new
	end
	unless $files[key[1]].key? key[0]
		$files[key[1]][key[0]] = Hash.new
		$syms[key[0]] = key[1]
	end
	#puts key[0] + " = '#{key[4]}'"
end

puts "digraph dump { node [shape=rect]; compound=true; nodesep=.1; ranksep=2; rankdir=LR; concentrate=true; "

modules = Hash.new;
links = Array.new;

1.upto(ARGV.length-1) { |i|

	use = File.new(ARGV[i])
#	puts "<<<<<<<<FILE " + ARGV[i]

	fil = ARGV[i].gsub(/.*\/|\.[ch]$/,"");

	while !use.eof?

		line = use.readline;
		next if line.match(/[ \t]*\/\//);
		line.gsub!(/[^a-zA-Z0-9_]/, " ");
		line.gsub!(/[ \t]+/, " ");
	#	puts ">>>" + line
		words = line.split(/[ \t]+/);
		words.each { |w|
			if $syms.key? w and $syms[w] != fil
				unless $files[$syms[w]][w].key? fil
	#				puts w + " is in " + $syms[w]
					$files[$syms[w]][w][fil] = 1

					sym=w
					unless modules.key? fil
						modules[fil] = Array.new
					end
					modules[fil] +=  [ "\"cc_#{fil}_#{sym}\" [label=\"#{sym}\",color=\"gray\",height=\".08\",style=dotted];" ]
					links += ["\"cc_#{fil}_#{sym}\" -> \"dd_#{$syms[w]}_#{sym}\";"]
				end
			end
		}
	end
}

$files.keys.each { |fil|
#	puts "File #{fil} ?"
	$files[fil].keys.each { |sym|
	#	puts "\tSym #{sym} : #{$files[fil][sym].length} ?"
		if $files[fil][sym].length > 0
			unless modules.key? fil
				modules[fil] = Array.new
			end
			modules[fil] +=  [ "\"dd_#{fil}_#{sym}\" [label=\"#{sym}\"];" ]
		end
	}
}
modules.keys.each {|fil|
	puts "\tsubgraph cluster_#{fil} {\n\t\tlabel=\"#{fil}\"; fontsize=\"18\"; "
	modules[fil].each { |lin|
		puts "\t\t#{lin}"
	}
	puts "\t}"
}
links.each { |lin|
	puts "\t#{lin}"
}
puts "}"
