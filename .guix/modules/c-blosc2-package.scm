;;; This file follows the suggestions in the article "From development
;;; environments to continuous integration—the ultimate guide to software
;;; development with Guix" by Ludovic Courtès at the Guix blog:
;;; <https://guix.gnu.org/es/blog/2023/from-development-environments-to-continuous-integrationthe-ultimate-guide-to-software-development-with-guix/>.

(define-module (c-blosc2-package)
  #:use-module (guix)
  #:use-module (guix build-system cmake)
  #:use-module (guix git-download)
  #:use-module ((guix licenses)
                #:prefix license:)
  #:use-module (gnu packages compression)
  #:use-module (ice-9 regex)
  #:use-module (ice-9 textual-ports))

(define (current-source-root)
  (dirname (dirname (current-source-directory))))

(define (get-c-blosc2-version)
  (let ((version-path (string-append (current-source-root) "/include/blosc2.h"))
        (version-rx (make-regexp
                     "^\\s*#\\s*define\\s*BLOSC2_VERSION_STRING\\s*\"([^\"]*)\".*"
                     regexp/newline)))
    (call-with-input-file version-path
      (lambda (port)
        (let* ((version-body (get-string-all port))
               (version-match (regexp-exec version-rx version-body)))
          (and version-match
               (match:substring version-match 1)))))))

(define vcs-file?
  ;; Return true if the given file is under version control.
  (or (git-predicate (current-source-root))
      (const #t)))

(define-public c-blosc2
  (package
    (name "c-blosc2")
    (version (get-c-blosc2-version))
    (source (local-file "../.."
                        "c-blosc2-checkout"
                        #:recursive? #t
                        #:select? (lambda (path stat)
                                    (and (vcs-file? path stat)
                                         (not (string-contains path
                                               "/internal-complibs"))))))
    (build-system cmake-build-system)
    (arguments
     ;; Disable AVX2 by default as in Guix' c-blosc package.
     `(#:configure-flags '("-DBUILD_STATIC=OFF"
                           "-DDEACTIVATE_AVX2=ON"
                           "-DDEACTIVATE_AVX512=ON"
                           "-DPREFER_EXTERNAL_LZ4=ON"
                           "-DPREFER_EXTERNAL_ZLIB=ON"
                           "-DPREFER_EXTERNAL_ZSTD=ON")))
    (inputs (list lz4 zlib
                  ;; The only input with a separate libs-only output.
                  `(,zstd "lib")))
    (home-page "https://blosc.org")
    (synopsis "Blocking, shuffling and lossless compression library")
    (description
     "Blosc is a high performance compressor optimized for binary
data (i.e. floating point numbers, integers and booleans, although it can
handle string data too).  It has been designed to transmit data to the
processor cache faster than the traditional, non-compressed, direct memory
fetch approach via a @code{memcpy()} system call.  Blosc main goal is not just
to reduce the size of large datasets on-disk or in-memory, but also to
accelerate memory-bound computations.

C-Blosc2 is the new major version of C-Blosc, and is backward compatible with
both the C-Blosc1 API and its in-memory format.  However, the reverse thing is
generally not true for the format; buffers generated with C-Blosc2 are not
format-compatible with C-Blosc1 (i.e. forward compatibility is not
supported).")
    (license license:bsd-3)))

(define (package-with-configure-flags p flags)
  "Return P with FLAGS as additional 'configure' flags."
  (package/inherit p
    (arguments (substitute-keyword-arguments (package-arguments p)
                 ((#:configure-flags original-flags
                   #~(list))
                  #~(append #$original-flags
                            #$flags))))))

(define-public c-blosc2-with-avx2
  (package
    (inherit (package-with-configure-flags c-blosc2
                                           #~(list "-DDEACTIVATE_AVX2=OFF")))
    (name "c-blosc2-with-avx2")))

(define-public c-blosc2-with-avx512
  (package
    (inherit (package-with-configure-flags c-blosc2
                                           #~(list "-DDEACTIVATE_AVX2=OFF"
                                                   "-DDEACTIVATE_AVX512=OFF")))
    (name "c-blosc2-with-avx512")))

c-blosc2
