function [n, d] = rat_max_den(x, max_den, tol)
  if nargin < 3
    tol = 1e-6 * norm(x(:), 1);
  end
  if nargin < 2
    max_den = 1000;
  end

  n = round(x);
  d = ones(size(x));
  x = x - n;

  for i = 1:numel(x)
    if abs(x(i)) > tol
      a = 0;
      n0 = 0; n1 = 1;
      d0 = 1; d1 = 0;
      while abs(x(i) - n1/d1) > tol && d1 < max_den
        a = floor(1/x(i));
        tmp = n1;
        n1 = a*n1 + n0;
        n0 = tmp;
        tmp = d1;
        d1 = a*d1 + d0;
        d0 = tmp;
        x(i) = 1/x(i) - a;
      end
      n(i) = n(i)*d1 + n1;
      d(i) = d1;
    end
  end
endfunction
