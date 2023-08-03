<?hh /* -*- php -*- */

namespace {
  const int FB_UNSERIALIZE_NONSTRING_VALUE;
  const int FB_UNSERIALIZE_UNEXPECTED_END;
  const int FB_UNSERIALIZE_UNRECOGNIZED_OBJECT_TYPE;
  const int FB_UNSERIALIZE_UNEXPECTED_ARRAY_KEY_TYPE;
  const int FB_UNSERIALIZE_MAX_DEPTH_EXCEEDED;

  const int FB_SERIALIZE_HACK_ARRAYS;
  const int FB_SERIALIZE_HACK_ARRAYS_AND_KEYSETS;
  const int FB_SERIALIZE_VARRAY_DARRAY;
  const int FB_SERIALIZE_POST_HACK_ARRAY_MIGRATION;

  const int FB_COMPACT_SERIALIZE_FORCE_PHP_ARRAYS;

  const int SETPROFILE_FLAGS_ENTERS;
  const int SETPROFILE_FLAGS_EXITS;
  const int SETPROFILE_FLAGS_DEFAULT;
  const int SETPROFILE_FLAGS_FRAME_PTRS;
  const int SETPROFILE_FLAGS_CTORS;
  const int SETPROFILE_FLAGS_RESUME_AWARE;
  /* This flag enables access to $this upon instance method entry in the
   * setprofile handler. It *may break* in the future. */
  const int SETPROFILE_FLAGS_THIS_OBJECT__MAY_BREAK;
  const int SETPROFILE_FLAGS_FILE_LINE;

  const int PREG_FB__PRIVATE__HSL_IMPL;

  <<__PHPStdLib>>
  function fb_serialize(
    HH\FIXME\MISSING_PARAM_TYPE $thing,
    int $options = 0,
  )[]: \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_unserialize(
    HH\FIXME\MISSING_PARAM_TYPE $thing,
    inout ?bool $success,
    int $options = 0,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_compact_serialize(
    HH\FIXME\MISSING_PARAM_TYPE $thing,
    int $options = 0,
  )[]: \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_compact_unserialize(
    HH\FIXME\MISSING_PARAM_TYPE $thing,
    inout ?bool $success,
    inout ?int $errcode,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_intercept2(
    string $name,
    HH\FIXME\MISSING_PARAM_TYPE $handler,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_rename_function(
    string $orig_func_name,
    string $new_func_name,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_utf8ize(
    inout string $input,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_utf8_strlen(string $input)[]: \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_utf8_substr(
    string $str,
    int $start,
    int $length = PHP_INT_MAX,
  )[]: \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_get_code_coverage(bool $flush): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_enable_code_coverage(): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_disable_code_coverage(): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_output_compression(
    bool $new_value,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_set_exit_callback(
    HH\FIXME\MISSING_PARAM_TYPE $function,
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_get_last_flush_size(): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function fb_setprofile(
    HH\FIXME\MISSING_PARAM_TYPE $callback,
    int $flags = SETPROFILE_FLAGS_DEFAULT,
    vec<string> $functions = vec[],
  ): \HH\FIXME\MISSING_RETURN_TYPE;

  function fb_call_user_func_async(
    string $initialDoc,
    mixed $function,
    mixed ...$func_args
  ): resource;
  function fb_gen_user_func(
    string $initialDoc,
    mixed $function,
    mixed ...$func_args
  ): Awaitable<dynamic>;
} // namespace

namespace HH {
  <<__PHPStdLib>>
  function disable_code_coverage_with_frequency(
  ): \HH\FIXME\MISSING_RETURN_TYPE;
  <<__PHPStdLib>>
  function non_crypto_md5_upper(string $str)[]: int;
  <<__PHPStdLib>>
  function non_crypto_md5_lower(string $str)[]: int;

  /** Returns the overflow part of multiplying two ints, as if they were unsigned.
   * In other words, this returns the upper 64 bits of the full product of
   * (unsigned)$a and (unsigned)$b. (The lower 64 bits is just `$a * $b`
   * regardless of signed/unsigned).
   */
  function int_mul_overflow(int $a, int $b): int;

  /** Returns the overflow part of multiplying two ints plus another int, as if
   * they were all unsigned. Specifically, this returns the upper 64 bits of
   * full (unsigned)$a * (unsigned)$b + (unsigned)$bias. $bias can be used to
   * manipulate rounding of the result.
   */
  function int_mul_add_overflow(int $a, int $b, int $bias): int;

  function enable_function_coverage(): \HH\FIXME\MISSING_RETURN_TYPE;

  function collect_function_coverage(): \HH\FIXME\MISSING_RETURN_TYPE;

  /**
   * Sets product attribution id into the caller's frame in order to be fetched
   * later down the call stack.
   */
  function set_product_attribution_id(int $id)[]: void;

  /**
   * Same as the above `set_product_attribution_id` function except it takes a
   * lambda that returns the attribution id to be called before fetching the value
   */
  function set_product_attribution_id_deferred((function()[leak_safe]: int) $fn)[]: void;

  /**
   * Fetches the closest product attribution id.
   * If no value is set, returns null.
   */
  function get_product_attribution_id()[leak_safe]: ?int;

  /**
   * Propagates the current product ID attribution into a lambda so that attempts
   * to retrieve attribution inside the lambda will return the creator's
   * attribution instead of the eventual caller's attribution.
   */
  function embed_product_attribution_id_in_closure<T>(
    (function ()[defaults]: T) $f,
  )[leak_safe]: (function ()[defaults]: T);

  /**
   * Propagates the current product ID attribution into an async lambda so that
   * attempts to retrieve attribution inside the lambda will return the creator's
   * attribution instead of the eventual caller's attribution.
   */
  function embed_product_attribution_id_in_async_closure<T>(
    (function ()[defaults]: Awaitable<T>) $f,
  )[leak_safe]: (function ()[defaults]: Awaitable<T>);

} // HH namespace
