TARGET=CaboCha
JAVAC=javac
JAVA=java
JAR=jar
CXX=c++
INCLUDE=/usr/lib/jvm/default-java/include

PACKAGE=org/chasen/cabocha

LIBS=`cabocha-config --libs`
INC=`cabocha-config --cflags` -I$(INCLUDE) -I$(INCLUDE)/linux

all:
	$(CXX) -O3 -c -fpic $(TARGET)_wrap.cxx  $(INC)
	$(CXX) -shared  $(TARGET)_wrap.o -o lib$(TARGET).so $(LIBS)
	$(JAVAC) -encoding utf8 $(PACKAGE)/*.java
	$(JAVAC) -encoding utf8 test.java
	$(JAR) cfv $(TARGET).jar $(PACKAGE)/*.class

test:
	env LD_LIBRARY_PATH=. $(JAVA) test

clean:
	rm -fr *.jar *.o *.so *.class $(PACKAGE)/*.class
	
cleanall:
	rm -fr $(TARGET).java *.cxx
