// RUN: %hackc compile-infer %s | FileCheck %s

// TEST-CHECK-BAL: define C._86pinit
// CHECK: define C._86pinit($this: *C) : *HackMixed {
// CHECK: #b0:
// CHECK:   ret null
// CHECK: }

class C {
  const A = 5;
}

// TEST-CHECK-BAL: define D._86pinit
// CHECK: define D._86pinit($this: *D) : *HackMixed {
// CHECK: #b0:
// CHECK:   n0: *D = load &$this
// CHECK:   n1 = C._86pinit(n0)
// CHECK:   ret null
// CHECK: }

class D extends C { }

// TEST-CHECK-BAL: define E._86pinit
// CHECK: define E._86pinit($this: *E) : *HackMixed {
// CHECK: #b0:
// CHECK:   n0: *E = load &$this
// CHECK:   n1 = D._86pinit(n0)
// CHECK:   n2 = &$this
// CHECK:   n3 = $builtins.hack_string("prop")
// CHECK:   n4 = $builtins.hack_dim_field_get(n2, n3)
// CHECK:   n5 = null
// CHECK:   store n4 <- n5: *HackMixed
// CHECK:   jmp b1, b2
// CHECK: #b1:
// CHECK:   prune $builtins.hack_is_true($builtins.hack_bool(false))
// CHECK:   jmp b3
// CHECK: #b2:
// CHECK:   prune ! $builtins.hack_is_true($builtins.hack_bool(false))
// CHECK:   n6 = __sil_lazy_class_initialize(<C>)
// CHECK:   n7 = $builtins.hack_field_get(n6, "A")
// CHECK:   n8 = &$this
// CHECK:   n9 = $builtins.hack_string("prop")
// CHECK:   n10 = $builtins.hack_dim_field_get(n8, n9)
// CHECK:   store n10 <- n7: *HackMixed
// CHECK:   jmp b3
// CHECK: #b3:
// CHECK:   ret null
// CHECK: }

class E extends D {
  public int $prop = C::A;
}
