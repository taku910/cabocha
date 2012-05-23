/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.40
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package org.chasen.cabocha;

public final class CharsetType {
  public final static CharsetType EUC_JP = new CharsetType("EUC_JP", CaboChaJNI.EUC_JP_get());
  public final static CharsetType CP932 = new CharsetType("CP932", CaboChaJNI.CP932_get());
  public final static CharsetType UTF8 = new CharsetType("UTF8", CaboChaJNI.UTF8_get());
  public final static CharsetType ASCII = new CharsetType("ASCII", CaboChaJNI.ASCII_get());

  public final int swigValue() {
    return swigValue;
  }

  public String toString() {
    return swigName;
  }

  public static CharsetType swigToEnum(int swigValue) {
    if (swigValue < swigValues.length && swigValue >= 0 && swigValues[swigValue].swigValue == swigValue)
      return swigValues[swigValue];
    for (int i = 0; i < swigValues.length; i++)
      if (swigValues[i].swigValue == swigValue)
        return swigValues[i];
    throw new IllegalArgumentException("No enum " + CharsetType.class + " with value " + swigValue);
  }

  private CharsetType(String swigName) {
    this.swigName = swigName;
    this.swigValue = swigNext++;
  }

  private CharsetType(String swigName, int swigValue) {
    this.swigName = swigName;
    this.swigValue = swigValue;
    swigNext = swigValue+1;
  }

  private CharsetType(String swigName, CharsetType swigEnum) {
    this.swigName = swigName;
    this.swigValue = swigEnum.swigValue;
    swigNext = this.swigValue+1;
  }

  private static CharsetType[] swigValues = { EUC_JP, CP932, UTF8, ASCII };
  private static int swigNext = 0;
  private final int swigValue;
  private final String swigName;
}
