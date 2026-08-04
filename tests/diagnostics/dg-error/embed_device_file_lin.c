/* dg-error: device files are not yet supported by '#embed' directive */
int zero[] = {
#embed "/dev/zero"
};
