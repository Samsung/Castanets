#
# This ProGuard configuration file illustrates how to process Scala
# applications, including the Scala runtime.
# Usage:
#     java -jar proguard.jar @scala.pro
#

# Specify the input jars, output jars, and library jars.

-injars  in.jar
-injars  /usr/local/java/scala-2.9.1/lib/scala-library.jar
#-injars  /usr/local/java/scala-2.9.1/lib/scala-compiler.jar(!META-INF/MANIFEST.MF)
#-injars  /usr/local/java/scala-2.9.1/lib/jline.jar(!META-INF/MANIFEST.MF)
-outjars out.jar

-libraryjars <java.home>/lib/rt.jar
#-libraryjars /usr/local/java/ant/lib/ant.jar
#...

# Ignore some compiler artefacts.

-dontwarn scala.**

# Save the obfuscation mapping to a file, so you can de-obfuscate any stack
# traces later on. Keep a fixed source file attribute and all line number
# tables to get line numbers in the stack traces.
# You can comment this out if you're not interested in stack traces.

-printmapping out.map
-renamesourcefileattribute SourceFile
-keepattributes SourceFile,LineNumberTable

# Preserve all annotations.

-keepattributes *Annotation*

# You can print out the seeds that are matching the keep options below.

#-printseeds out.seeds

# Preserve all public applications.

-keepclasseswithmembers public class * {
    public static void main(java.lang.String[]);
}

# Preserve some classes and class members that are accessed by means of
# introspection.

-keep class * implements org.xml.sax.EntityResolver

-keepclassmembers class * {
    ** MODULE$;
}

-keepclassmembernames class scala.concurrent.forkjoin.ForkJoinPool {
    long eventCount;
    int  workerCounts;
    int  runControl;
    scala.concurrent.forkjoin.ForkJoinPool$WaitQueueNode syncStack;
    scala.concurrent.forkjoin.ForkJoinPool$WaitQueueNode spareStack;
}

-keepclassmembernames class scala.concurrent.forkjoin.ForkJoinWorkerThread {
    int base;
    int sp;
    int runState;
}

-keepclassmembernames class scala.concurrent.forkjoin.ForkJoinTask {
    int status;
}

-keepclassmembernames class scala.concurrent.forkjoin.LinkedTransferQueue {
    scala.concurrent.forkjoin.LinkedTransferQueue$PaddedAtomicReference head;
    scala.concurrent.forkjoin.LinkedTransferQueue$PaddedAtomicReference tail;
    scala.concurrent.forkjoin.LinkedTransferQueue$PaddedAtomicReference cleanMe;
}

# Preserve some classes and class members that are accessed by means of
# introspection in the Scala compiler library, if it is processed as well.

#-keep class * implements jline.Completor
#-keep class * implements jline.Terminal

#-keep class scala.tools.nsc.Global

#-keepclasseswithmembers class * {
#    <init>(scala.tools.nsc.Global);
#}

#-keepclassmembers class * {
#    *** scala_repl_value();
#    *** scala_repl_result();
#}

# Preserve all native method names and the names of their classes.

-keepclasseswithmembernames class * {
    native <methods>;
}

# Preserve the special static methods that are required in all enumeration
# classes.

-keepclassmembers class * extends java.lang.Enum {
    public static **[] values();
    public static ** valueOf(java.lang.String);
}

# Explicitly preserve all serialization members. The Serializable interface
# is only a marker interface, so it wouldn't save them.
# You can comment this out if your application doesn't use serialization.
# If your code contains serializable classes that have to be backward 
# compatible, please refer to the manual.

-keepclassmembers class * implements java.io.Serializable {
    static final long serialVersionUID;
    static final java.io.ObjectStreamField[] serialPersistentFields;
    private void writeObject(java.io.ObjectOutputStream);
    private void readObject(java.io.ObjectInputStream);
    java.lang.Object writeReplace();
    java.lang.Object readResolve();
}

# Your application may contain more items that need to be preserved; 
# typically classes that are dynamically created using Class.forName:

# -keep public class mypackage.MyClass
# -keep public interface mypackage.MyInterface
# -keep public class * implements mypackage.MyInterface

