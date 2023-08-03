//// modules.php
<?hh

new module A {}
new module B {}

//// A.php
<?hh

<<file: __EnableUnstableFeatures('module_level_traits')>>

// basic module level traits behaviour:
// - T belongs to module A even if used by class C in B
// - getFoo can thus safely invoke foo

module A;

internal function foo(): void { echo "foo\n"; }

<<__ModuleLevelTrait>>
public trait T {
  public function getFoo(): void {
    foo();
  }
}

//// B.php
<?hh

module B;

class C {
  use T;
}

<<__EntryPoint>>
function bar(): void {
  (new C())->getFoo();
}
