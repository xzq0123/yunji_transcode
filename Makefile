# 顶层 Makefile

# 定义子目录
SUBDIRS := sample/ppl

.PHONY: all install clean $(SUBDIRS)

# 默认目标
all: $(SUBDIRS)

# 递归调用子目录的 Makefile
$(SUBDIRS):
	$(MAKE) -C $@ host=x86 all install

install: all

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

