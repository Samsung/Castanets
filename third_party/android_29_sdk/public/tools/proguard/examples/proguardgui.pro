#
# This ProGuard configuration file illustrates how to process the ProGuard GUI.
# Configuration files for typical applications will be very similar.
# Usage:
#     java -jar proguard.jar @proguardgui.pro
#

# Specify the input jars, output jars, and library jars.
# The input jars will be merged in a single output jar.
# We'll filter out the Ant and WTK classes.

-injars  ../lib/proguardgui.jar
-injars  ../lib/proguard.jar(!META-INF/**,!proguard/ant/**,!proguard/wtk/**)
-injars  ../lib/retrace.jar (!META-INF/**)
-outjars proguardgui_out.jar

-libraryjars <java.home>/lib/rt.jar

# If we wanted to reuse the previously obfuscated proguard_out.jar, we could
# perform incremental obfuscation based on its mapping file, and only keep the
# additional GUI files instead of all files.

#-applymapping proguard.map
#-injars      ../lib/proguardgui.jar
#-outjars     proguardgui_out.jar
#-libraryjars ../lib/proguard.jar(!proguard/ant/**,!proguard/wtk/**)
#-libraryjars ../lib/retrace.jar
#-libraryjars <java.home>/lib/rt.jar


# Allow methods with the same signature, except for the return type,
# to get the same obfuscation name.

-overloadaggressively

# Put all obfuscated classes into the nameless root package.

-repackageclasses ''

# Adapt the names of resource files, based on the corresponding obfuscated
# class names. Notably, in this case, the GUI resource properties file will
# have to be renamed.

-adaptresourcefilenames **.properties,**.gif,**.jpg

# The entry point: ProGuardGUI and its main method.

-keep public class proguard.gui.ProGuardGUI {
    public static void main(java.lang.String[]);
}
