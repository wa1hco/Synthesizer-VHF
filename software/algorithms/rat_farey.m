%% Farey sequence-based rational approximation of numbers.
%% Per Magnusson, 2024, 2025
%% MIT licence, http://www.opensource.org/licenses/mit-license.php
%% Modified for Octave by wa1hco, 2025
%% https://axotron.se/blog/fast-algorithm-for-rational-approximation-of-floating-point-numbers/
%% Find the best rational approximation to a number between 0 and 1.
%%
%% target - a number between 0 and 1 (inclusive)
%% maxdenom - the maximum allowed denominator
%%
%% The algorithm is based on Farey sequences/fractions. See
%% https://web.archive.org/web/20181119092100/https://nrich.maths.org/6596
%% a, b, c, d notation from
%% https://en.wikipedia.org/wiki/Farey_sequence is used here (not
%% from the above reference). I.e. narrow the interval between a/b
%% and c/d by splitting it using the mediant (a+c)/(b+d) until we are
%% close enough with either endpoint, or we have a denominator that is
%% bigger than what is allowed.
%% Start with the interval 0 to 1 (i.e. 0/1 to 1/1).
%% A simple implementation of just calculating the mediant (a+c)/(b+d) and
%% iterating with the mediant replacing the worst value of a/b and c/d is very
%% inefficient in cases where the target is close to a rational number
%% with a small denominator, like e.g. when approximating 10^-6.
%% The straightforward algorithm would need about 10^6 iterations as it
%% would try all of 1/1, 1/2, 1/3, 1/4, 1/5 etc. To resolve this slow
%% convergence, at each step, it is calculated how many times the
%% interval will need to be narrowed from the same side and all those
%% steps are taken at once.
function [n, d, ii] = rat_farey(target, maxdenom)

  a = 0; b = 1; c = 1; d = 1; ii = 0;
  maxIter = 100;

  if target > 1
    n = 1;
    d = 1;
    return;
  endif
  if target < 0
    n = 1;
    d = 1;
    return;
  endif

  mediant = 0;
  Ndenom_min = 1 / (10 * maxdenom);
  ii = 0;
  while 1
    ac = a + c;
    bd = b + d;
    if bd > maxdenom || ii > maxIter
      if target - a/b < c/d - target
        ac = a;
        bd = b;
      else
        ac = c;
        bd = d;
      endif
      break;
    endif
    ii = ii + 1;
    mediant = ac / bd;
    if target < mediant
      Ndenom = target * b - a;
      if Ndenom < Ndenom_min
        ac = a;
        bd = b;
        break;
      endif
      N = (c - target * d) / Ndenom;
      Nint = floor(N);
      if Nint < 1
        Nint = 1;
      endif
      if d + Nint * b > maxdenom
        N = (maxdenom - d) / b;
        Nint = floor(N);
      endif
      c = c + Nint * a;
      d = d + Nint * b;
    else
      Ndenom = c - target * d;
      if Ndenom < Ndenom_min
        ac = c;
        bd = d;
        break;
      endif
      N = (target * b - a) / Ndenom;
      Nint = floor(N);
      if Nint < 1
        Nint = 1;
      endif
      if b + Nint * d > maxdenom
        N = (maxdenom - b) / d;
        Nint = floor(N);
      endif
      a = a + Nint * c;
      b = b + Nint * d;
    endif
  endwhile
  n = ac;
  d = bd;
  return;
endfunction
