BINS = clang-mutate
MANS = $(BINS:=.1.gz)

all: $(MANS)

%.1.gz: %.md
	pandoc -s -t man $< -o $<.tmp
	gzip -9 < $<.tmp > $@

clean:
	rm -f $(MANS) $(BINS:=.md.tmp)
