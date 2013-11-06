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

# For the callgraph, you need Graphviz, ruby and exuberant ctags.
# The manual requires latex including additional packages (acronym, graphicx, fancyvrb, ...).
# This is not generated in the normal code build

.PHONY: manual

all:	simavr_callgraph.pdf manual

simavr_callgraph.pdf:
	ctags -f .tags ../simavr/sim/sim_*.[ch] ../simavr/sim/run_*.[ch] 2>/dev/null && \
		ruby ./tags_to_dot.rb .tags \
			../simavr/sim/sim_*.c ../simavr/sim/run_*.c >.tags.dot && \
				dot -Tpdf .tags.dot -o $@

manual:
	$(MAKE) -C manual

clean:
	rm -f .tags*
	$(MAKE) -C manual clean
