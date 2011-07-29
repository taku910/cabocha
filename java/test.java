import org.chasen.cabocha.CaboCha;
import org.chasen.cabocha.Parser;
import org.chasen.cabocha.Tree;
import org.chasen.cabocha.FormatType;

public class test {
  static {
    try {
       System.loadLibrary("CaboCha");
    } catch (UnsatisfiedLinkError e) {
       System.err.println("Cannot load the example native code.\nMake sure your LD_LIBRARY_PATH contains \'.\'\n" + e);
       System.exit(1);
    }
  }

  public static void main(String[] argv) {
     System.out.println(CaboCha.VERSION);
//     Parser parser = new Parser("-f2");
     Parser parser = new Parser();     
     String str =  "太郎は二郎にこの本を渡した.";
     System.out.println(parser.parseToString(str));
     Tree tree = parser.parse(str);
     System.out.println(tree.toString(FormatType.FORMAT_TREE));
     System.out.println(tree.toString(FormatType.FORMAT_LATTICE));
  }
}
